package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import com.mapswithme.maps.R;

public class BookmarkCategoriesActivity extends AbstractBookmarkCategoryActivity
{

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    ListView listView = getListView();
    listView.setAdapter(new BookmarkCategoriesAdapter(this));
    listView.setOnItemClickListener(new OnItemClickListener()
    {

      @Override
      public void onItemClick(AdapterView<?> parent, View view, int position, long id)
      {
        startActivity(new Intent(BookmarkCategoriesActivity.this, BookmarkListActivity.class).putExtra(BookmarkActivity.PIN_SET,
            position));
      }
    });
    registerForContextMenu(getListView());
  }

  @Override
  public void onCreateContextMenu(ContextMenu menu, View v,
                                  ContextMenuInfo menuInfo)
  {
    getMenuInflater().inflate(R.menu.bookmark_categories_context_menu, menu);
    super.onCreateContextMenu(menu, v, menuInfo);
  }

  @Override
  protected boolean enableEditing()
  {
    return true;
  }
}
