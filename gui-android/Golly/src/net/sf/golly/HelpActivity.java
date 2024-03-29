/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com

 / ***/

package net.sf.golly;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.support.v4.app.NavUtils;
import android.util.DisplayMetrics;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.Toast;

public class HelpActivity extends BaseActivity {

    // see jnicalls.cpp for these native routines:
    private static native void nativeClassInit();   // this MUST be static
    private native void nativeCreate();             // the rest must NOT be static
    private native void nativeDestroy();
    private native void nativeGetURL(String url, String pageurl);
    private native void nativeUnzipFile(String zippath);
    private native boolean nativeDownloadedFile(String url);

    // local fields:
    private static boolean firstcall = true;
    private static Bundle webbundle = null;         // for saving/restoring scroll position and page history
    private boolean restarted = false;              // onRestart was called before OnResume?
    private boolean clearhistory = false;           // tell onPageFinished to clear page history?
    private WebView gwebview;                       // for displaying html pages
    private Button backbutton;                      // go to previous page
    private Button nextbutton;                      // go to next page
    private static String pageurl;                  // URL for last displayed page
    private ProgressBar progbar;                    // progress bar for downloads
    private LinearLayout proglayout;                // view containing progress bar
    private boolean cancelled;                      // download was cancelled?

    public final static String SHOWHELP_MESSAGE = "net.sf.golly.SHOWHELP";
    
    // -----------------------------------------------------------------------------
    
    static {
        nativeClassInit();      // caches Java method IDs
    }
        
    // -----------------------------------------------------------------------------
    
    // this class lets us intercept link taps
    private class MyWebViewClient extends WebViewClient {
        @Override
        public boolean shouldOverrideUrlLoading(WebView webview, String url) {
            // look for special prefixes used by Golly and return true if found
            if (url.startsWith("open:")) {
                openFile(url.substring(5));
                return true;
            }
            if (url.startsWith("edit:")) {
                editFile(url.substring(5));
                return true;
            }
            if (url.startsWith("rule:")) {
                changeRule(url.substring(5));
                return true;
            }
            if (url.startsWith("lexpatt:")) {
                // user tapped on pattern in Life Lexicon
                loadLexiconPattern(url.substring(8));
                return true;
            }
            if (url.startsWith("get:")) {
                // download file specifed in link (possibly relative to a previous full url)
                nativeGetURL(url.substring(4), pageurl);
                return true;
            }
            if (url.startsWith("unzip:")) {
                nativeUnzipFile(url.substring(6));
                return true;
            }
            // no special prefix, so look for file with .zip/rle/lif/mc extension and download it
            if (nativeDownloadedFile(url)) {
                return true;
            }
            return false;
        }
        
        @Override  
        public void onPageFinished(WebView webview, String url) {
            super.onPageFinished(webview, url);
            
            if (clearhistory) {
                // this only works after page is loaded
                gwebview.clearHistory();
                clearhistory = false;
            }
            backbutton.setEnabled(gwebview.canGoBack());
            nextbutton.setEnabled(gwebview.canGoForward());

            // need URL of this page for relative "get:" links
            pageurl = gwebview.getUrl();
            // Log.i("URL", pageurl);
        }  
    }

    // -----------------------------------------------------------------------------
    
    private void openFile(String filepath) {
        // switch to main screen and open given file
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.OPENFILE_MESSAGE, filepath);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------
    
    private void editFile(String filepath) {
        // let user read/edit given file
        if (filepath.startsWith("Supplied/")) {
            // read contents of supplied .rule file into a string
            String prefix = MainActivity.supplieddir.getAbsolutePath();
            prefix = prefix.substring(0, prefix.length() - 8);  // strip off "Supplied"
            String fullpath = prefix + filepath;
            File file = new File(fullpath);
            String filecontents;
            try {
                FileInputStream instream = new FileInputStream(file);
                BufferedReader reader = new BufferedReader(new InputStreamReader(instream));
                StringBuilder sb = new StringBuilder();
                String line = null;
                while ((line = reader.readLine()) != null) {
                    sb.append(line);
                    sb.append("\n");
                }
                filecontents = sb.toString();
                instream.close();
            } catch (Exception e) {
                filecontents = "Error reading file:\n" + e.toString();
            }
            // display filecontents
            Intent intent = new Intent(this, InfoActivity.class);
            intent.putExtra(InfoActivity.INFO_MESSAGE, filecontents);
            startActivity(intent);
        } else {
            // let user read or edit user's .rule file
            String fullpath = MainActivity.userdir.getAbsolutePath() + "/" + filepath;
            Intent intent = new Intent(this, EditActivity.class);
            intent.putExtra(EditActivity.EDITFILE_MESSAGE, fullpath);
            startActivity(intent);
        }
    }

    // -----------------------------------------------------------------------------
    
    private void changeRule(String rule) {
        // switch to main screen and change rule
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.RULE_MESSAGE, rule);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------
    
    private void loadLexiconPattern(String pattern) {
        // switch to main screen and load given lexicon pattern
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.LEXICON_MESSAGE, pattern);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.help_layout);
        backbutton = (Button) findViewById(R.id.back);
        nextbutton = (Button) findViewById(R.id.forwards);
        progbar = (ProgressBar) findViewById(R.id.progress_bar);
        proglayout = (LinearLayout) findViewById(R.id.progress_layout);
        
        proglayout.setVisibility(LinearLayout.INVISIBLE);
        
        // show the Up button in the action bar
        getActionBar().setDisplayHomeAsUpEnabled(true);
        
        nativeCreate();     // cache this instance
        
        gwebview = (WebView) findViewById(R.id.webview);
        gwebview.setWebViewClient(new MyWebViewClient());
        
        // JavaScript is used to detect device type
        gwebview.getSettings().setJavaScriptEnabled(true);
        
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        // my Nexus 7 has a density of 320
        if (metrics.densityDpi > 300) {
            // use bigger font size for high density screens (default size is 16)
            gwebview.getSettings().setDefaultFontSize(24);
        }
        
        if (firstcall) {
            firstcall = false;
            backbutton.setEnabled(false);
            nextbutton.setEnabled(false);
            showContentsPage();
            // pageurl has now been initialized
        }
    }
    
    // -----------------------------------------------------------------------------
    
    @Override
    protected void onPause() {
        super.onPause();
        gwebview.onPause();
        
        // save scroll position and back/forward history
        if (webbundle == null) webbundle = new Bundle();
        gwebview.saveState(webbundle);
    }

    // -----------------------------------------------------------------------------
    
    @Override
    protected void onResume() {
        super.onResume();
        gwebview.onResume();
        
        // restore scroll position and back/forward history
        if (webbundle != null && !restarted) {
            gwebview.restoreState(webbundle);
        }
        restarted = false;
        
        // check for messages sent by other activities
        Intent intent = getIntent();
        String filepath = intent.getStringExtra(SHOWHELP_MESSAGE);
        if (filepath != null) {
            gwebview.loadUrl("file://" + filepath);
        } else {
            gwebview.reload();
        }
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onRestart() {
        super.onRestart();
        // set flag to prevent onResume calling gwebview.restoreState
        // causing app to crash for some unknown reason
        restarted = true;
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onDestroy() {
        nativeDestroy();
        super.onDestroy();
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // add main.xml items to the action bar
        getMenuInflater().inflate(R.menu.main, menu);
        
        // disable the item for this activity
        MenuItem item = menu.findItem(R.id.help);
        item.setEnabled(false);
        
        return true;
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // action bar item has been tapped
        Intent intent;
        switch (item.getItemId()) {
            case android.R.id.home:
                // the Home or Up button will start MainActivity
                NavUtils.navigateUpFromSameTask(this);
                return true;
            case R.id.open:
                intent = new Intent(this, OpenActivity.class);
                startActivity(intent);
                return true;
            case R.id.settings:
                intent = new Intent(this, SettingsActivity.class);
                startActivity(intent);
                return true;
            case R.id.help:
                // do nothing
                break;
        }
        return super.onOptionsItemSelected(item);
    }
    
    // -----------------------------------------------------------------------------
    
    private void showContentsPage() {
        // display html data in supplieddir/Help/index.html
        String fullpath = MainActivity.supplieddir.getAbsolutePath() + "/Help/index.html";
        File htmlfile = new File(fullpath);
        if (htmlfile.exists()) {
            gwebview.loadUrl("file://" + fullpath);     // fullpath starts with "/"
        } else {
            // should never happen
            String htmldata = "<html><center><font size=+1 color=\"red\">Failed to find index.html!</font></center></html>";
            gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Contents button is tapped
    public void doContents(View view) {
        clearhistory = true;    // tell onPageFinished to clear page history
        showContentsPage();
        if (webbundle != null) {
            webbundle.clear();
            webbundle = null;
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when backbutton is tapped
    public void doBack(View view) {
        gwebview.goBack();
    }
    
    // -----------------------------------------------------------------------------
    
    // called when nextbutton is tapped
    public void doForwards(View view) {
        gwebview.goForward();
    }
    
    // -----------------------------------------------------------------------------
    
    // called when Cancel is tapped
    public void doCancel(View view) {
        cancelled = true;
    }
    
    // -----------------------------------------------------------------------------

    private String downloadURL(String urlstring, String filepath) {
        // download given url and save data in given file
        try {
            File outfile = new File(filepath);
            final int BUFFSIZE = 8192;
            FileOutputStream outstream = null;
            try {
                outstream = new FileOutputStream(outfile);
            } catch (FileNotFoundException e) {
                return "File not found: " + filepath;
            }
            
            long starttime = System.nanoTime();
            
            // Log.i("downloadURL: ", urlstring);
            URL url = new URL(urlstring);
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setAllowUserInteraction(false);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestMethod("GET");
            connection.connect();
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                outstream.close();
                return "No HTTP_OK response.";
            }
            
            // init info for progress bar
            int filesize = connection.getContentLength();
            int downloaded = 0;
            int percent;
            int lastpercent = 0;
            
            // stream the data to given file
            InputStream instream = connection.getInputStream();
            byte[] buffer = new byte[BUFFSIZE];
            int bufflen = 0;
            while ((bufflen = instream.read(buffer, 0, BUFFSIZE)) > 0) {
                outstream.write(buffer, 0, bufflen);
                downloaded += bufflen;
                percent = (int) ((downloaded / (float)filesize) * 100);
                if (percent > lastpercent) {
                    progbar.setProgress(percent);
                    lastpercent = percent;
                }
                // show proglayout only if download takes more than 1 second
                if (System.nanoTime() - starttime > 1000000000L) {
                    runOnUiThread(new Runnable() {
                        public void run() {
                            proglayout.setVisibility(LinearLayout.VISIBLE);
                        }
                    });
                    starttime = Long.MAX_VALUE; // only show it once
                }
                if (cancelled) break;
            }
            outstream.close();
            connection.disconnect();
            if (cancelled) return "Cancelled.";
            
        } catch (MalformedURLException e) {
            return "Bad URL string: " + urlstring;
        } catch (IOException e) {
            return "Could not connect to URL: " + urlstring;
        }
        return "";  // success
    }
    
    // -----------------------------------------------------------------------------

    private String dresult;
    
    // this method is called from C++ code (see jnicalls.cpp)
    private String DownloadFile(String urlstring, String filepath) {
        // we cannot do network connections on main thread, so we do the
        // download on a new thread, but we have to wait for it to finish
        final Handler handler = new Handler() {
            public void handleMessage(Message msg) {
                throw new RuntimeException();
            } 
        };
        
        cancelled = false;
        progbar.setProgress(0);
        // don't show proglayout immediately
        // proglayout.setVisibility(LinearLayout.VISIBLE);
        
        dresult = "";
        final String durl = urlstring;
        final String dfile = filepath;
        Thread download_thread = new Thread(new Runnable() {
            public void run() {
                dresult = downloadURL(durl, dfile);
                handler.sendMessage(handler.obtainMessage());
            }
        });
        
        download_thread.setPriority(Thread.MAX_PRIORITY);
        download_thread.start();
        
        // wait for thread to finish
        try { Looper.loop(); } catch(RuntimeException re) {}
        
        proglayout.setVisibility(LinearLayout.INVISIBLE);
        
        if (dresult.length() > 0 && !cancelled) {
            Toast.makeText(this, "Download failed! " + dresult, Toast.LENGTH_SHORT).show();
        }
        return dresult;
    }

} // HelpActivity class
