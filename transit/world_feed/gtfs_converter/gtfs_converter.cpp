#include "transit/world_feed/color_picker.hpp"
#include "transit/world_feed/world_feed.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/timer.hpp"

#include "3party/gflags/src/gflags/gflags.h"

DEFINE_string(path_mapping, "", "Path to the mapping file of TransitId to GTFS hash");
DEFINE_string(path_gtfs_feeds, "", "Directory with GTFS feeds subdirectories");
DEFINE_string(path_json, "", "Output directory for dumping json files");
DEFINE_string(path_resources, "", "MAPS.ME resources directory");
DEFINE_string(start_feed, "", "Optional. Feed directory from which the process continues");
DEFINE_string(stop_feed, "", "Optional. Feed directory on which to stop the process");

// Finds subdirectories with feeds.
Platform::FilesList GetGtfsFeedsInDirectory(std::string const & path)
{
  Platform::FilesList res;
  Platform::TFilesWithType gtfsList;
  Platform::GetFilesByType(path, Platform::FILE_TYPE_DIRECTORY, gtfsList);

  for (auto const & item : gtfsList)
  {
    auto const & gtfsFeedDir = item.first;
    if (gtfsFeedDir != "." && gtfsFeedDir != "..")
      res.push_back(base::JoinPath(path, gtfsFeedDir));
  }

  return res;
}

// Handles the case when the directory consists of a single subdirectory with GTFS files.
void ExtendPath(std::string & path)
{
  Platform::TFilesWithType csvFiles;
  Platform::GetFilesByType(path, Platform::FILE_TYPE_REGULAR, csvFiles);
  if (!csvFiles.empty())
    return;

  Platform::TFilesWithType subdirs;
  Platform::GetFilesByType(path, Platform::FILE_TYPE_DIRECTORY, subdirs);

  // If there are more subdirectories then ".", ".." and directory with feed, the feed is most
  // likely corrupted.
  if (subdirs.size() > 3)
    return;

  for (auto const & item : subdirs)
  {
    auto const & subdir = item.first;
    if (subdir != "." && subdir != "..")
    {
      path = base::JoinPath(path, subdir);
      LOG(LDEBUG, ("Found subdirectory with feed", path));
      return;
    }
  }
}

bool SkipFeed(std::string const & feedPath, bool & pass)
{
  if (!FLAGS_start_feed.empty() && pass)
  {
    if (base::GetNameFromFullPath(feedPath) != FLAGS_start_feed)
      return true;
    pass = false;
  }
  return false;
}

bool StopOnFeed(std::string const & feedPath)
{
  if (!FLAGS_stop_feed.empty() && base::GetNameFromFullPath(feedPath) == FLAGS_stop_feed)
  {
    LOG(LINFO, ("Stop on", feedPath));
    return true;
  }
  return false;
}

enum class FeedStatus
{
  OK = 0,
  CORRUPTED,
  NO_SHAPES
};

FeedStatus ReadFeed(gtfs::Feed & feed)
{
  // First we read shapes. If there are no shapes in feed we do not need to read all the required
  // files - agencies, stops, etc.
  if (auto res = feed.read_shapes(); res != gtfs::ResultCode::OK)
  {
    LOG(LWARNING, ("Could not get shapes.", res.message));
    return FeedStatus::NO_SHAPES;
  }

  if (feed.get_shapes().empty())
    return FeedStatus::NO_SHAPES;

  // We try to parse required for json files and return error in case of invalid file content.
  if (auto res = feed.read_agencies(); res != gtfs::ResultCode::OK)
  {
    LOG(LWARNING, ("Could not parse agencies.", res.message));
    return FeedStatus::CORRUPTED;
  }

  if (auto res = feed.read_routes(); res != gtfs::ResultCode::OK)
  {
    LOG(LWARNING, ("Could not parse routes.", res.message));
    return FeedStatus::CORRUPTED;
  }

  if (auto res = feed.read_trips(); res != gtfs::ResultCode::OK)
  {
    LOG(LWARNING, ("Could not parse trips.", res.message));
    return FeedStatus::CORRUPTED;
  }

  if (auto res = feed.read_stops(); res != gtfs::ResultCode::OK)
  {
    LOG(LWARNING, ("Could not parse stops.", res.message));
    return FeedStatus::CORRUPTED;
  }

  if (auto res = feed.read_stop_times(); res != gtfs::ResultCode::OK)
  {
    LOG(LWARNING, ("Could not parse stop times.", res.message));
    return FeedStatus::CORRUPTED;
  }

  // We try to parse optional for json files and do not return error in case of invalid file
  // content, only log warning message.
  if (auto res = feed.read_calendar(); gtfs::ErrorParsingOptionalFile(res))
    LOG(LINFO, ("Could not parse calendar.", res.message));

  if (auto res = feed.read_calendar_dates(); gtfs::ErrorParsingOptionalFile(res))
    LOG(LINFO, ("Could not parse calendar dates.", res.message));

  if (auto res = feed.read_frequencies(); gtfs::ErrorParsingOptionalFile(res))
    LOG(LINFO, ("Could not parse frequencies.", res.message));

  if (auto res = feed.read_transfers(); gtfs::ErrorParsingOptionalFile(res))
    LOG(LINFO, ("Could not parse transfers.", res.message));

  if (feed.read_feed_info() == gtfs::ResultCode::OK)
    LOG(LINFO, ("Feed info is present."));

  return FeedStatus::OK;
}

int main(int argc, char ** argv)
{
  google::SetUsageMessage("Reads GTFS feeds, produces json with global ids for generator.");
  google::ParseCommandLineFlags(&argc, &argv, true);
  auto const toolName = base::GetNameFromFullPath(argv[0]);

  if (FLAGS_path_mapping.empty() || FLAGS_path_gtfs_feeds.empty() || FLAGS_path_json.empty())
  {
    LOG(LWARNING, ("Some of the required options are not present."));
    google::ShowUsageWithFlagsRestrict(argv[0], toolName.c_str());
    return -1;
  }

  if (!Platform::IsDirectory(FLAGS_path_gtfs_feeds) || !Platform::IsDirectory(FLAGS_path_json) ||
      !Platform::IsDirectory(FLAGS_path_resources))
  {
    LOG(LWARNING,
        ("Some paths set in options are not valid. Check the directories:",
         FLAGS_path_gtfs_feeds, FLAGS_path_json, FLAGS_path_resources));
    google::ShowUsageWithFlagsRestrict(argv[0], toolName.c_str());
    return -1;
  }

  auto const gtfsFeeds = GetGtfsFeedsInDirectory(FLAGS_path_gtfs_feeds);

  if (gtfsFeeds.empty())
  {
    LOG(LERROR, ("No subdirectories with GTFS feeds found in", FLAGS_path_gtfs_feeds));
    return -1;
  }

  std::vector<std::string> invalidFeeds;

  size_t feedsWithNoShapesCount = 0;
  size_t feedsNotDumpedCount = 0;
  size_t feedsDumped = 0;
  size_t feedsTotal = gtfsFeeds.size();
  bool pass = true;

  transit::IdGenerator generator(FLAGS_path_mapping);

  GetPlatform().SetResourceDir(FLAGS_path_resources);
  transit::ColorPicker colorPicker;

  for (size_t i = 0; i < gtfsFeeds.size(); ++i)
  {
    base::Timer feedTimer;
    auto feedPath = gtfsFeeds[i];

    if (SkipFeed(feedPath, pass))
    {
      ++feedsTotal;
      LOG(LINFO, ("Skipped", feedPath));
      continue;
    }

    bool stop = StopOnFeed(feedPath);
    if (stop)
      feedsTotal -= (gtfsFeeds.size() - i - 1);

    ExtendPath(feedPath);
    LOG(LINFO, ("Handling feed", feedPath));

    gtfs::Feed feed(feedPath);

    if (auto const res = ReadFeed(feed); res != FeedStatus::OK)
    {
      if (res == FeedStatus::NO_SHAPES)
        feedsWithNoShapesCount++;
      else
        invalidFeeds.push_back(feedPath);

      if (stop)
        break;
      continue;
    }

    transit::WorldFeed globalFeed(generator, colorPicker);

    if (!globalFeed.SetFeed(std::move(feed)))
    {
      LOG(LINFO, ("Error transforming feed for json representation."));
      ++feedsNotDumpedCount;
      if (stop)
        break;
      continue;
    }

    bool const saved = globalFeed.Save(FLAGS_path_json, i == 0 /* overwrite */);
    if (saved)
      ++feedsDumped;
    else
      ++feedsNotDumpedCount;

    LOG(LINFO, ("Merged:", saved ? "yes" : "no", "time", feedTimer.ElapsedSeconds(), "s"));

    if (stop)
      break;
  }

  generator.Save();

  LOG(LINFO, ("Corrupted feeds paths:", invalidFeeds));
  LOG(LINFO, ("Corrupted feeds:", invalidFeeds.size(), "/", feedsTotal));
  LOG(LINFO, ("Feeds with no shapes:", feedsWithNoShapesCount, "/", feedsTotal));
  LOG(LINFO, ("Feeds parsed but not dumped:", feedsNotDumpedCount, "/", feedsTotal));
  LOG(LINFO, ("Total dumped feeds:", feedsDumped, "/", feedsTotal));
  LOG(LINFO, ("Bad stop sequences:", transit::WorldFeed::GetCorruptedStopSequenceCount()));
  return 0;
}
