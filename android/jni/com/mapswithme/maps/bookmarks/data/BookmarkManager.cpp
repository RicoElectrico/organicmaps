#include "../../Framework.hpp"

#include "../../../core/jni_helper.hpp"

namespace
{
  ::Framework * frm() { return g_framework->NativeFramework(); }
}

extern "C"
{

  jstring getFormattedNameForPlace(JNIEnv * env, Framework::AddressInfo const & adInfo)
  {
    if (adInfo.m_name.empty())
      return jni::ToJavaString(env, adInfo.GetBestType());
    else
    {
      std::ostringstream stringStream;
      stringStream << adInfo.m_name << " (" << adInfo.GetBestType() << ")";
      return jni::ToJavaString(env, stringStream.str());
    }
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nGetNameForPlace(JNIEnv * env, jobject thiz, jint px, jint py)
  {
    Framework::AddressInfo adInfo;
    frm()->GetAddressInfo(m2::PointD(px, py), adInfo);
    return getFormattedNameForPlace(env, adInfo);
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nGetNameForPOI(JNIEnv * env, jobject thiz, jint px, jint py)
  {
    Framework::AddressInfo adInfo;
    m2::PointD pxPivot;
    frm()->GetVisiblePOI(m2::PointD(px, py), pxPivot, adInfo);
    return getFormattedNameForPlace(env, adInfo);
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nFindVisiblePOI(JNIEnv * env, jobject thiz, jint px, jint py)
  {
    Framework::AddressInfo adInfo;
    m2::PointD pxPivot;
    return frm()->GetVisiblePOI(m2::PointD(px, py), pxPivot, adInfo);
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nGetBmkPositionForPOI(JNIEnv * env, jobject thiz, jint px, jint py)
  {
    Framework::AddressInfo adInfo;
    m2::PointD pxPivot;
    if (frm()->GetVisiblePOI(m2::PointD(px, py), pxPivot, adInfo))
      return jni::GetNewPoint(env, pxPivot);
    else
      return jni::GetNewPoint(env, m2::PointI(px, py));
  }


  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nShowBookmark(JNIEnv * env, jobject thiz, jint c, jint b)
  {
    frm()->ShowBookmark(*(frm()->GetBmCategory(c)->GetBookmark(b)));
    frm()->SaveState();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_putBookmark(
      JNIEnv * env, jobject thiz, jint px, jint py, jstring bookmarkName, jstring categoryName)
  {
    Bookmark bm(frm()->PtoG(m2::PointD(px, py)), jni::ToNativeString(env, bookmarkName), "placemark-red");
    frm()->AddBookmark(jni::ToNativeString(env, categoryName), bm)->SaveToKMLFile();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nLoadBookmarks(
      JNIEnv * env, jobject thiz)
  {
    frm()->LoadBookmarks();
  }


  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_getCategoriesCount(
       JNIEnv * env, jobject thiz)
  {
    return frm()->GetBmCategoriesCount();
  }

  //TODO rename
  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nGetCategoryByName(
      JNIEnv * env, jobject thiz, jstring name)
 {
   return frm()->IsCategoryExist(jni::ToNativeString(env, name));
 }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nDeleteCategory(
       JNIEnv * env, jobject thiz, jint index)
  {
    return frm()->DeleteBmCategory(index);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nDeleteBookmark(JNIEnv * env, jobject thiz, jint cat, jint bmk)
  {
    BookmarkCategory * pCat = frm()->GetBmCategory(cat);
    if (pCat)
    {
      pCat->DeleteBookmark(bmk);
      pCat->SaveToKMLFile();
    }
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_bookmarks_data_BookmarkManager_nGetBookmark(JNIEnv * env, jobject thiz, jint px, jint py)
  {
    BookmarkAndCategory bac = frm()->GetBookmark(m2::PointD(px, py));

    return jni::GetNewPoint(env, m2::PointI(bac.first, bac.second));
   }
}
