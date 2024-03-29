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

#include <jni.h>        // for calling Java from C++ and vice versa
#include <GLES/gl.h>    // for OpenGL ES 1.x calls
#include <unistd.h>     // for usleep
#include <string>       // for std::string
#include <list>         // for std::list
#include <set>          // for std::set
#include <algorithm>    // for std::count

#include "utils.h"      // for Warning, etc
#include "algos.h"      // for InitAlgorithms
#include "prefs.h"      // for GetPrefs, SavePrefs, userdir, etc
#include "layer.h"      // for AddLayer, ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent
#include "file.h"       // for NewPattern
#include "render.h"     // for DrawPattern
#include "view.h"       // for widescreen, fullscreen, TouchBegan, etc
#include "status.h"     // for UpdateStatusLines, ClearMessage, etc
#include "undo.h"       // for ClearUndoRedo
#include "jnicalls.h"

// -----------------------------------------------------------------------------

// these globals cache info for calling methods in .java files
static JavaVM* javavm;
static jobject mainobj = NULL;          // current instance of MainActivity
static jmethodID main_StartMainActivity;
static jmethodID main_RefreshPattern;
static jmethodID main_ShowStatusLines;
static jmethodID main_UpdateEditBar;
static jmethodID main_CheckMessageQueue;
static jmethodID main_PlayBeepSound;
static jmethodID main_Warning;
static jmethodID main_Fatal;
static jmethodID main_YesNo;
static jmethodID main_RemoveFile;
static jmethodID main_MoveFile;
static jmethodID main_CopyTextToClipboard;
static jmethodID main_GetTextFromClipboard;
static jmethodID main_ShowHelp;
static jmethodID main_ShowTextFile;
static jmethodID main_BeginProgress;
static jmethodID main_AbortProgress;
static jmethodID main_EndProgress;

static jobject helpobj = NULL;          // current instance of HelpActivity
static jmethodID help_DownloadFile;

static bool rendering = false;          // in DrawPattern?
static bool paused = false;             // generating has been paused?
static bool touching_pattern = false;   // is pattern being touched?
static bool highdensity = false;        // screen density is > 300dpi?

// -----------------------------------------------------------------------------

// XPM data for digits 0 to 9 where each digit is a 10x10 icon
// so we can use CreateIconBitmaps

static const char* digits[] = {
/* width height ncolors chars_per_pixel */
"10 100 16 1",
/* colors */
"A c #FFFFFF",
"B c #EEEEEE",
"C c #DDDDDD",
"D c #CCCCCC",
"E c #BBBBBB",
"F c #AAAAAA",
"G c #999999",
"H c #888888",
"I c #777777",
"J c #666666",
"K c #555555",
"L c #444444",
"M c #333333",
"N c #222222",
"O c #111111",
". c #000000",    // black will be transparent
/* pixels */
"AAAAAA....",
"AGMMBA....",
"CMBFKA....",
"HIAAOA....",
"JFAANB....",
"IFAANA....",
"FJACLA....",
"ALLODA....",
"AACAAA....",
"AAAAAA....",
"AAAAAA....",
"AAFIAA....",
"AIOIAA....",
"AGKIAA....",
"AAGIAA....",
"AAGIAA....",
"AAGIAA....",
"AAGIAA....",
"AAAAAA....",
"AAAAAA....",
"AAAAAA....",
"AINLFA....",
"EMADNA....",
"FGAAOB....",
"AABIJA....",
"AEMGAA....",
"ELAAAA....",
"IONMNB....",
"AAAAAA....",
"AAAAAA....",
"AAAAAA....",
"AKNLDA....",
"FKAGJA....",
"FDAELA....",
"AAJOFA....",
"BAAEOA....",
"IFABNA....",
"CMLLFA....",
"AABBAA....",
"AAAAAA....",
"AAAAAA....",
"AABNDA....",
"AAJODA....",
"AEKLDA....",
"BMBKDA....",
"KIFMHB....",
"EFGMHB....",
"AAAKDA....",
"AAAAAA....",
"AAAAAA....",
"AAAAAA....",
"ANNNKA....",
"BLBBBA....",
"DJCBAA....",
"FNJLHA....",
"ABABNB....",
"FFABMA....",
"CMKLFA....",
"AABAAA....",
"AAAAAA....",
"AAAAAA....",
"AFLMFA....",
"BMBCMA....",
"GICBCA....",
"IMKMHA....",
"IJAANB....",
"GJAANA....",
"AJLLFA....",
"AABAAA....",
"AAAAAA....",
"AAAAAA....",
"JNNNND....",
"BCCEMB....",
"AAAKEA....",
"AAEKAA....",
"AAMCAA....",
"ADLAAA....",
"AHHAAA....",
"AAAAAA....",
"AAAAAA....",
"AAAAAA....",
"AINLDA....",
"DMAGKA....",
"EKADLA....",
"BMNOFA....",
"HJADNA....",
"HHAANB....",
"BMLLGA....",
"AACAAA....",
"AAAAAA....",
"AAAAAA....",
"AINLBA....",
"FKBGKA....",
"IGABNA....",
"FJBFOB....",
"AGKIMA....",
"EFADKA....",
"CLKMCA....",
"AABAAA....",
"AAAAAA...."};

static gBitmapPtr* digits10x10;    // digit bitmaps for displaying state numbers

// -----------------------------------------------------------------------------

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    javavm = vm;    // save in global

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("GetEnv failed!");
        return -1;
    }

    return JNI_VERSION_1_6;
}

// -----------------------------------------------------------------------------

static JNIEnv* getJNIenv(bool* attached)
{
    // get the JNI environment for the current thread
    JNIEnv* env;
    *attached = false;

    int result = javavm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        if (javavm->AttachCurrentThread(&env, NULL) != 0) {
            LOGE("AttachCurrentThread failed!");
            return NULL;
        }
        *attached = true;
    } else if (result == JNI_EVERSION) {
        LOGE("GetEnv: JNI_VERSION_1_6 is not supported!");
        return NULL;
    }

    return env;
}

// -----------------------------------------------------------------------------

static std::string ConvertJString(JNIEnv* env, jstring str)
{
   const jsize len = env->GetStringUTFLength(str);
   const char* strChars = env->GetStringUTFChars(str, 0);
   std::string result(strChars, len);
   env->ReleaseStringUTFChars(str, strChars);
   return result;
}

// -----------------------------------------------------------------------------

static void CheckIfRendering()
{
    int msecs = 0;
    while (rendering) {
        // wait for DrawPattern to finish
        usleep(1000);    // 1 millisec
        msecs++;
    }
    // if (msecs > 0) LOGI("waited for rendering: %d msecs", msecs);
}

// -----------------------------------------------------------------------------

void UpdatePattern()
{
    // call a Java method that calls GLSurfaceView.requestRender() which calls nativeRender
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_RefreshPattern);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void UpdateStatus()
{
    if (fullscreen) return;

    UpdateStatusLines();    // sets status1, status2, status3

    // call Java method in MainActivity class to update the status bar info
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_ShowStatusLines);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void PauseGenerating()
{
    if (generating) {
        StopGenerating();
        // generating is now false
        paused = true;
    }
}

// -----------------------------------------------------------------------------

void ResumeGenerating()
{
    if (paused) {
        StartGenerating();
        // generating is probably true (false if pattern is empty)
        paused = false;
    }
}

// =============================================================================

// these native routines are used in MainActivity.java:

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeClassInit(JNIEnv* env, jclass klass)
{
    // get IDs for Java methods in MainActivity
    main_StartMainActivity = env->GetMethodID(klass, "StartMainActivity", "()V");
    main_RefreshPattern = env->GetMethodID(klass, "RefreshPattern", "()V");
    main_ShowStatusLines = env->GetMethodID(klass, "ShowStatusLines", "()V");
    main_UpdateEditBar = env->GetMethodID(klass, "UpdateEditBar", "()V");
    main_CheckMessageQueue = env->GetMethodID(klass, "CheckMessageQueue", "()V");
    main_PlayBeepSound = env->GetMethodID(klass, "PlayBeepSound", "()V");
    main_Warning = env->GetMethodID(klass, "Warning", "(Ljava/lang/String;)V");
    main_Fatal = env->GetMethodID(klass, "Fatal", "(Ljava/lang/String;)V");
    main_YesNo = env->GetMethodID(klass, "YesNo", "(Ljava/lang/String;)Ljava/lang/String;");
    main_RemoveFile = env->GetMethodID(klass, "RemoveFile", "(Ljava/lang/String;)V");
    main_MoveFile = env->GetMethodID(klass, "MoveFile", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    main_CopyTextToClipboard = env->GetMethodID(klass, "CopyTextToClipboard", "(Ljava/lang/String;)V");
    main_GetTextFromClipboard = env->GetMethodID(klass, "GetTextFromClipboard", "()Ljava/lang/String;");
    main_ShowHelp = env->GetMethodID(klass, "ShowHelp", "(Ljava/lang/String;)V");
    main_ShowTextFile = env->GetMethodID(klass, "ShowTextFile", "(Ljava/lang/String;)V");
    main_BeginProgress = env->GetMethodID(klass, "BeginProgress", "(Ljava/lang/String;)V");
    main_AbortProgress = env->GetMethodID(klass, "AbortProgress", "(ILjava/lang/String;)Z");
    main_EndProgress = env->GetMethodID(klass, "EndProgress", "()V");
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeCreate(JNIEnv* env, jobject obj)
{
    // save obj for calling Java methods in this instance of MainActivity
    if (mainobj != NULL) env->DeleteGlobalRef(mainobj);
    mainobj = env->NewGlobalRef(obj);

    static bool firstcall = true;
    if (firstcall) {
        firstcall = false;          // only do the following once
        SetMessage("This is Golly 1.0 for Android.  Copyright 2013 The Golly Gang.");
        if (highdensity) {
            MAX_MAG = 6;            // maximum cell size = 64x64
        } else {
            MAX_MAG = 5;            // maximum cell size = 32x32
        }
        InitAlgorithms();           // must initialize algoinfo first
        GetPrefs();                 // load user's preferences
        SetMinimumStepExponent();   // for slowest speed
        AddLayer();                 // create initial layer (sets currlayer)
        NewPattern();               // create new, empty universe
        UpdateStatus();             // show initial message

        digits10x10 = CreateIconBitmaps(digits,11);     // 1st "digit" is not used
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeDestroy(JNIEnv* env)
{
    // the current instance of MainActivity is being destroyed
    if (mainobj != NULL) {
        env->DeleteGlobalRef(mainobj);
        mainobj = NULL;
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_MainActivity_nativeGetStatusLine(JNIEnv* env, jobject obj, jint line)
{
    switch (line) {
        case 1: return env->NewStringUTF(status1.c_str());
        case 2: return env->NewStringUTF(status2.c_str());
        case 3: return env->NewStringUTF(status3.c_str());
    }
    return env->NewStringUTF("Fix bug in nativeGetStatusLine!");
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeGetStatusColor(JNIEnv* env)
{
    unsigned char r = algoinfo[currlayer->algtype]->statusrgb.r;
    unsigned char g = algoinfo[currlayer->algtype]->statusrgb.g;
    unsigned char b = algoinfo[currlayer->algtype]->statusrgb.b;
    // return status bar color as int in format 0xAARRGGBB
    return (0xFF000000 | (r << 16) | (g << 8) | b);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_MainActivity_nativeGetPasteMode(JNIEnv* env)
{
    return env->NewStringUTF(GetPasteMode());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_MainActivity_nativeGetRandomFill(JNIEnv* env)
{
    char s[4];    // room for 0..100
    sprintf(s, "%d", randomfill);
    return env->NewStringUTF(s);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_MainActivity_nativeGetInfo(JNIEnv* env)
{
    std::string info;
    if (currlayer->currfile.empty()) {
        // should never happen
        info = "There is no current pattern file!";
    } else {
        // get comments in current pattern file
        char *commptr = NULL;
        // readcomments will allocate commptr
        const char *err = readcomments(currlayer->currfile.c_str(), &commptr);
        if (err) {
            info = err;
        } else if (commptr[0] == 0) {
            info = "No comments found.";
        } else {
            info = commptr;
        }
        if (commptr) free(commptr);
    }
    return env->NewStringUTF(info.c_str());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetUserDirs(JNIEnv* env, jobject obj, jstring path)
{
    userdir = ConvertJString(env, path) + "/";
    // userdir = /mnt/sdcard/Android/data/net.sf.golly/files if external storage
    // is available, otherwise /data/data/net.sf.golly/files/ on internal storage

    // set paths to various subdirs (created by caller)
    userrules = userdir + "Rules/";
    savedir = userdir + "Saved/";
    downloaddir = userdir + "Downloads/";
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetSuppliedDirs(JNIEnv* env, jobject obj, jstring path)
{
    supplieddir = ConvertJString(env, path) + "/";
    // supplieddir = /data/data/net.sf.golly/files/Supplied/
    
    // set paths to various subdirs (created by caller)
    helpdir = supplieddir + "Help/";
    rulesdir = supplieddir + "Rules/";
    patternsdir = supplieddir + "Patterns/";
    
    // also set prefsfile here by replacing "Supplied/" with "GollyPrefs"
    prefsfile = supplieddir.substr(0, supplieddir.length() - 9) + "GollyPrefs";
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetTempDir(JNIEnv* env, jobject obj, jstring path)
{
    tempdir = ConvertJString(env, path) + "/";
    // tempdir = /data/data/net.sf.golly/cache/
    clipfile = tempdir + "golly_clipboard";

    /* assume nativeSetTempDir is called last
    LOGI("userdir =     %s", userdir.c_str());
    LOGI("savedir =     %s", savedir.c_str());
    LOGI("downloaddir = %s", downloaddir.c_str());
    LOGI("userrules =   %s", userrules.c_str());
    LOGI("supplieddir = %s", supplieddir.c_str());
    LOGI("patternsdir = %s", patternsdir.c_str());
    LOGI("rulesdir =    %s", rulesdir.c_str());
    LOGI("helpdir =     %s", helpdir.c_str());
    LOGI("tempdir =     %s", tempdir.c_str());
    LOGI("clipfile =    %s", clipfile.c_str());
    LOGI("prefsfile =   %s", prefsfile.c_str());
    */
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeAllowUndo(JNIEnv* env)
{
    return allowundo;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeCanUndo(JNIEnv* env)
{
    return currlayer->undoredo->CanUndo();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeCanRedo(JNIEnv* env)
{
    return currlayer->undoredo->CanRedo();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeInfoAvailable(JNIEnv* env)
{
    return currlayer->currname != "untitled";
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeUndo(JNIEnv* env)
{
    if (generating) Warning("Bug: generating is true in nativeUndo!");
    CheckIfRendering();
    currlayer->undoredo->UndoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRedo(JNIEnv* env)
{
    if (generating) Warning("Bug: generating is true in nativeRedo!");
    CheckIfRendering();
    currlayer->undoredo->RedoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeCanReset(JNIEnv* env)
{
    return currlayer->algo->getGeneration() > currlayer->startgen;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeResetPattern(JNIEnv* env)
{
    CheckIfRendering();
    ResetPattern();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativePauseGenerating(JNIEnv* env)
{
    PauseGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeResumeGenerating(JNIEnv* env)
{
    ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStartGenerating(JNIEnv* env)
{
    if (!generating) {
        StartGenerating();
        // generating might still be false (eg. if pattern is empty)

        // best to reset paused flag in case a PauseGenerating call
        // wasn't followed by a matching ResumeGenerating call
        paused = false;
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStopGenerating(JNIEnv* env)
{
    if (generating) {
        StopGenerating();
        // generating is now false
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeIsGenerating(JNIEnv* env)
{
    return generating;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeGenerate(JNIEnv* env)
{
    if (paused) return;     // PauseGenerating has been called

    if (touching_pattern) return;     // avoid jerky pattern updates

    // play safe and avoid re-entering currlayer->algo->step()
    if (event_checker > 0) return;

    // avoid calling NextGeneration while DrawPattern is executing on different thread
    if (rendering) return;

    NextGeneration(true);   // calls currlayer->algo->step() using current gen increment
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStep(JNIEnv* env)
{
    NextGeneration(true);
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeCalculateSpeed(JNIEnv* env)
{
    // calculate the interval (in millisecs) between nativeGenerate calls

    int interval = 1000/60;        // max speed is 60 fps

    // increase interval if user wants a delay
    if (currlayer->currexpo < 0) {
        interval = GetCurrentDelay();
    }

    return interval;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStep1(JNIEnv* env)
{
    // reset step exponent to 0
    currlayer->currexpo = 0;
    SetGenIncrement();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFaster(JNIEnv* env)
{
    // go faster by incrementing step exponent
    currlayer->currexpo++;
    SetGenIncrement();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSlower(JNIEnv* env)
{
    // go slower by decrementing step exponent
    if (currlayer->currexpo > minexpo) {
        currlayer->currexpo--;
        SetGenIncrement();
    } else {
        Beep();
    }
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStopBeforeNew(JNIEnv* env)
{
    // NewPattern is about to clear all undo/redo history so there's no point
    // saving the current pattern (which might be very large)
    bool saveundo = allowundo;
    allowundo = false;                    // avoid calling RememberGenFinish
    if (generating) StopGenerating();
    allowundo = saveundo;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeNewPattern(JNIEnv* env)
{
    CheckIfRendering();
    NewPattern();
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFitPattern(JNIEnv* env)
{
    CheckIfRendering();
    FitInView(1);
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeScale1to1(JNIEnv* env)
{
    CheckIfRendering();
    // set scale to 1:1
    if (currlayer->view->getmag() != 0) {
        currlayer->view->setmag(0);
        UpdatePattern();
        UpdateStatus();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeBigger(JNIEnv* env)
{
    CheckIfRendering();
    // zoom in
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom();
        UpdatePattern();
        UpdateStatus();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSmaller(JNIEnv* env)
{
    CheckIfRendering();
    // zoom out
    currlayer->view->unzoom();
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeMiddle(JNIEnv* env)
{
    if (currlayer->originx == bigint::zero && currlayer->originy == bigint::zero) {
        currlayer->view->center();
    } else {
        // put cell saved by ChangeOrigin (not yet implemented!!!) in middle
        currlayer->view->setpositionmag(currlayer->originx, currlayer->originy, currlayer->view->getmag());
    }
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeGetMode(JNIEnv* env)
{
    switch (currlayer->touchmode) {
        case drawmode:   return 0;
        case pickmode:   return 1;
        case selectmode: return 2;
        case movemode:   return 3;
    }
    Warning("Bug detected in nativeGetMode!");
    return 0;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetMode(JNIEnv* env, jobject obj, jint mode)
{
    switch (mode) {
        case 0: currlayer->touchmode = drawmode;   return;
        case 1: currlayer->touchmode = pickmode;   return;
        case 2: currlayer->touchmode = selectmode; return;
        case 3: currlayer->touchmode = movemode;   return;
    }
    Warning("Bug detected in nativeSetMode!");
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeNumLayers(JNIEnv* env)
{
    return numlayers;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativePasteExists(JNIEnv* env)
{
    return waitingforpaste;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeSelectionExists(JNIEnv* env)
{
    return currlayer->currsel.Exists();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativePaste(JNIEnv* env)
{
    CheckIfRendering();
    PasteClipboard();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSelectAll(JNIEnv* env)
{
    SelectAll();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRemoveSelection(JNIEnv* env)
{
    RemoveSelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeCutSelection(JNIEnv* env)
{
    CheckIfRendering();
    CutSelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeCopySelection(JNIEnv* env)
{
    CopySelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeClearSelection(JNIEnv* env, jobject obj, jint inside)
{
    CheckIfRendering();
    if (inside) {
        ClearSelection();
    } else {
        ClearOutsideSelection();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeShrinkSelection(JNIEnv* env)
{
    ShrinkSelection(false);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFitSelection(JNIEnv* env)
{
    FitSelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRandomFill(JNIEnv* env)
{
    CheckIfRendering();
    RandomFill();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFlipSelection(JNIEnv* env, jobject obj, jint y)
{
    CheckIfRendering();
    FlipSelection(y);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRotateSelection(JNIEnv* env, jobject obj, jint clockwise)
{
    CheckIfRendering();
    RotateSelection(clockwise);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeAdvanceSelection(JNIEnv* env, jobject obj, jint inside)
{
    CheckIfRendering();
    if (inside) {
        currlayer->currsel.Advance();
    } else {
        currlayer->currsel.AdvanceOutside();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeAbortPaste(JNIEnv* env)
{
    AbortPaste();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeDoPaste(JNIEnv* env, jobject obj, jint toselection)
{
    // assume caller has stopped generating
    CheckIfRendering();
    DoPaste(toselection);
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFlipPaste(JNIEnv* env, jobject obj, jint y)
{
    FlipPastePattern(y);
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRotatePaste(JNIEnv* env, jobject obj, jint clockwise)
{
    RotatePastePattern(clockwise);
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeClearMessage(JNIEnv* env)
{
    ClearMessage();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_MainActivity_nativeGetValidExtensions(JNIEnv* env)
{
    if (currlayer->algo->hyperCapable()) {
        // .rle format is allowed but .mc format is better
        return env->NewStringUTF(".mc (the default) or .mc.gz or .rle or .rle.gz");
    } else {
        // only .rle format is allowed
        return env->NewStringUTF(".rle (the default) or .rle.gz");
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeValidExtension(JNIEnv* env, jobject obj, jstring filename)
{
    std::string fname = ConvertJString(env, filename);

    size_t dotpos = fname.find('.');
    if (dotpos == std::string::npos) return true;   // no extension given (default will be added later)

    if (EndsWith(fname,".rle")) return true;
    if (EndsWith(fname,".rle.gz")) return true;
    if (currlayer->algo->hyperCapable()) {
        if (EndsWith(fname,".mc")) return true;
        if (EndsWith(fname,".mc.gz")) return true;
    }

    return false;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeFileExists(JNIEnv* env, jobject obj, jstring filename)
{
    std::string fname = ConvertJString(env, filename);

    // append default extension if not supplied
    size_t dotpos = fname.find('.');
    if (dotpos == std::string::npos) {
        if (currlayer->algo->hyperCapable()) {
            fname += ".mc";
        } else {
            fname += ".rle";
        }
    }

    std::string fullpath = savedir + fname;
    return FileExists(fullpath);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSavePattern(JNIEnv* env, jobject obj, jstring filename)
{
    std::string fname = ConvertJString(env, filename);

    // append default extension if not supplied
    size_t dotpos = fname.find('.');
    if (dotpos == std::string::npos) {
        if (currlayer->algo->hyperCapable()) {
            fname += ".mc";
        } else {
            fname += ".rle";
        }
    }

    pattern_format format = XRLE_format;
    if (EndsWith(fname,".mc") || EndsWith(fname,".mc.gz")) format = MC_format;

    output_compression compression = no_compression;
    if (EndsWith(fname,".gz")) compression = gzip_compression;

    std::string fullpath = savedir + fname;
    SavePattern(fullpath, format, compression);
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeOpenFile(JNIEnv* env, jobject obj, jstring filepath)
{
    std::string fpath = ConvertJString(env, filepath);
    FixURLPath(fpath);
    OpenFile(fpath.c_str());
    SavePrefs();                // save recentpatterns
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetScreenDensity(JNIEnv* env, jobject obj, jint dpi)
{
    highdensity = dpi > 300;    // my Nexus 7 has a density of 320dpi
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetWideScreen(JNIEnv* env, jobject obj, jboolean iswide)
{
    widescreen = iswide;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetFullScreen(JNIEnv* env, jobject obj, jboolean isfull)
{
    fullscreen = isfull;
    if (!fullscreen) {
        // user might have changed scale or position while in fullscreen mode
        UpdateStatus();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeChangeRule(JNIEnv* env, jobject obj, jstring rule)
{
    std::string newrule = ConvertJString(env, rule);
    ChangeRule(newrule);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeLexiconPattern(JNIEnv* env, jobject obj, jstring jpattern)
{
    std::string pattern = ConvertJString(env, jpattern);
    std::replace(pattern.begin(), pattern.end(), '$', '\n');
    LoadLexiconPattern(pattern);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeDrawingState(JNIEnv* env)
{
    return currlayer->drawingstate;
}

// =============================================================================

// these native routines are used in PatternGLSurfaceView.java:

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativePause(JNIEnv* env)
{
    PauseGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeResume(JNIEnv* env)
{
    ResumeGenerating();
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeTouchBegan(JNIEnv* env, jobject obj, jint x, jint y)
{
    // LOGI("touch began: x=%d y=%d", x, y);
    ClearMessage();
    TouchBegan(x, y);

    // set flag so we can avoid jerky updates if pattern is generating
    touching_pattern = true;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeTouchMoved(JNIEnv* env, jobject obj, jint x, jint y)
{
    // LOGI("touch moved: x=%d y=%d", x, y);
    TouchMoved(x, y);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeTouchEnded(JNIEnv* env)
{
    // LOGI("touch ended");
    TouchEnded();

    touching_pattern = false;   // allow pattern generation
}

// -----------------------------------------------------------------------------

static TouchModes oldmode;

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeMoveMode(JNIEnv* env)
{
    // temporarily switch touch mode to movemode
    oldmode = currlayer->touchmode;
    currlayer->touchmode = movemode;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeRestoreMode(JNIEnv* env)
{
    // restore touch mode saved in nativeMoveMode
    currlayer->touchmode = oldmode;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeZoomIn(JNIEnv* env, jobject obj, jint x, jint y)
{
    ZoomInPos(x, y);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeZoomOut(JNIEnv* env, jobject obj, jint x, jint y)
{
    ZoomOutPos(x, y);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternRenderer_nativeInit(JNIEnv* env)
{
    // we only do 2D drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FOG);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    glEnable(GL_BLEND);
    // this blending function seems similar to the one used in desktop Golly
    // (ie. selected patterns look much the same)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternRenderer_nativeResize(JNIEnv* env, jobject obj, jint w, jint h)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, w, h, 0, -1, 1);     // origin is top left and y increases down
    glViewport(0, 0, w, h);
    glMatrixMode(GL_MODELVIEW);

    if (w != currlayer->view->getwidth() || h != currlayer->view->getheight()) {
        // update size of viewport
        ResizeLayers(w, h);
        UpdatePattern();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternRenderer_nativeRender(JNIEnv* env)
{
    // if NextGeneration is executing (on different thread) then don't call DrawPattern
    if (event_checker > 0) return;

    // render the current pattern; note that this occurs on a different thread
    // so we use rendering flag to prevent calls like NewPattern being called
    // before DrawPattern has finished
    rendering = true;
    DrawPattern(currindex);
    rendering = false;
}

// =============================================================================

// these native routines are used in OpenActivity.java:

const char* HTML_HEADER = "<html><font color='black'><b>";
const char* HTML_FOOTER = "</b></font></html>";
const char* HTML_INDENT = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

static std::set<std::string> opendirs;      // set of open directories in Supplied patterns

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_OpenActivity_nativeGetRecentPatterns(JNIEnv* env)
{
    std::string htmldata = HTML_HEADER;
    if (recentpatterns.empty()) {
        htmldata += "There are no recent patterns.";
    } else {
        htmldata += "Recently opened/saved patterns:<br><br>";
        std::list<std::string>::iterator next = recentpatterns.begin();
        while (next != recentpatterns.end()) {
            std::string path = *next;
            if (path.find("Patterns/") == 0 || FileExists(userdir + path)) {
                htmldata += "<a href=\"open:";
                htmldata += path;
                htmldata += "\">";
                if (path.find("Patterns/") == 0) {
                    // nicer not to show Patterns/ prefix
                    size_t firstsep = path.find('/');
                    if (firstsep != std::string::npos) {
                        path.erase(0, firstsep+1);
                    }
                }
                htmldata += path;
                htmldata += "</a><br>";
            }
            next++;
        }
    }
    htmldata += HTML_FOOTER;
    return env->NewStringUTF(htmldata.c_str());
}

// -----------------------------------------------------------------------------

static void AppendHtmlData(std::string& htmldata, const std::string& paths, const std::string& dir,
                           const std::string& prefix, bool candelete)
{
    int closedlevel = 0;
    size_t pathstart = 0;
    size_t pathend = paths.find('\n');

    while (pathend != std::string::npos) {
        std::string path = paths.substr(pathstart, pathend - pathstart);
        // path is relative to given dir (eg. "Life/Bounded-Grids/agar-p3.rle" if patternsdir)

        bool isdir = (paths[pathend-1] == '/');
        if (isdir) {
            // strip off terminating '/'
            path = paths.substr(pathstart, pathend - pathstart - 1);
        }

        // set indent level to number of separators in path
        int indents = std::count(path.begin(), path.end(), '/');

        if (indents <= closedlevel) {
            if (isdir) {
                // path is to a directory
                std::string imgname;
                if (opendirs.find(path) == opendirs.end()) {
                    closedlevel = indents;
                    imgname = "triangle-right.png";
                } else {
                    closedlevel = indents+1;
                    imgname = "triangle-down.png";
                }
                for (int i = 0; i < indents; i++) htmldata += HTML_INDENT;
                htmldata += "<a href=\"toggledir:";
                htmldata += path;
                htmldata += "\"><img src=\"";
                htmldata += imgname;
                htmldata += "\" border=0/><font color=\"gray\">";
                size_t lastsep = path.rfind('/');
                if (lastsep == std::string::npos) {
                    htmldata += path;
                } else {
                    htmldata += path.substr(lastsep+1);
                }
                htmldata += "</font></a><br>";
            } else {
                // path is to a file
                for (int i = 0; i < indents; i++) htmldata += HTML_INDENT;
                if (candelete) {
                    // allow user to delete file
                    htmldata += "<a href=\"delete:";
                    htmldata += prefix;
                    htmldata += path;
                    htmldata += "\"><font size=-2 color='red'>DELETE</font></a>&nbsp;&nbsp;&nbsp;";
                    // allow user to edit file
                    htmldata += "<a href=\"edit:";
                    htmldata += prefix;
                    htmldata += path;
                    htmldata += "\"><font size=-2 color='green'>EDIT</font></a>&nbsp;&nbsp;&nbsp;";
                } else {
                    // allow user to read file (a supplied pattern)
                    htmldata += "<a href=\"edit:";
                    htmldata += prefix;
                    htmldata += path;
                    htmldata += "\"><font size=-2 color='green'>READ</font></a>&nbsp;&nbsp;&nbsp;";
                }
                htmldata += "<a href=\"open:";
                htmldata += prefix;
                htmldata += path;
                htmldata += "\">";
                size_t lastsep = path.rfind('/');
                if (lastsep == std::string::npos) {
                    htmldata += path;
                } else {
                    htmldata += path.substr(lastsep+1);
                }
                htmldata += "</a><br>";
            }
        }

        pathstart = pathend + 1;
        pathend = paths.find('\n', pathstart);
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_OpenActivity_nativeGetSavedPatterns(JNIEnv* env, jobject obj, jstring jpaths)
{
    std::string paths = ConvertJString(env, jpaths);
    std::string htmldata = HTML_HEADER;
    if (paths.length() == 0) {
        htmldata += "There are no saved patterns.";
    } else {
        htmldata += "Saved patterns:<br><br>";
        AppendHtmlData(htmldata, paths, savedir, "Saved/", true);   // can delete files
    }
    htmldata += HTML_FOOTER;
    return env->NewStringUTF(htmldata.c_str());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_OpenActivity_nativeGetDownloadedPatterns(JNIEnv* env, jobject obj, jstring jpaths)
{
    std::string paths = ConvertJString(env, jpaths);
    std::string htmldata = HTML_HEADER;
    if (paths.length() == 0) {
        htmldata += "There are no downloaded patterns.";
    } else {
        htmldata += "Downloaded patterns:<br><br>";
        AppendHtmlData(htmldata, paths, downloaddir, "Downloads/", true);  // can delete files
    }
    htmldata += HTML_FOOTER;
    return env->NewStringUTF(htmldata.c_str());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_OpenActivity_nativeGetSuppliedPatterns(JNIEnv* env, jobject obj, jstring jpaths)
{
    std::string paths = ConvertJString(env, jpaths);
    std::string htmldata = HTML_HEADER;
    if (paths.length() == 0) {
        htmldata += "There are no supplied patterns.";
    } else {
        htmldata += "Supplied patterns:<br><br>";
        AppendHtmlData(htmldata, paths, patternsdir, "Patterns/", false);   // can't delete files
    }
    htmldata += HTML_FOOTER;
    return env->NewStringUTF(htmldata.c_str());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_OpenActivity_nativeToggleDir(JNIEnv* env, jobject obj, jstring jpath)
{
    std::string path = ConvertJString(env, jpath);
    if (opendirs.find(path) == opendirs.end()) {
        // add directory path to opendirs
        opendirs.insert(path);
    } else {
        // remove directory path from opendirs
        opendirs.erase(path);
    }
}

// =============================================================================

// these native routines are used in SettingsActivity.java:

static bool oldcolors;      // detect if user changed swapcolors
static bool oldundo;        // detect if user changed allowundo
static bool oldhashinfo;    // detect if user changed currlayer->showhashinfo
static int oldhashmem;      // detect if user changed maxhashmem

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_SettingsActivity_nativeOpenSettings(JNIEnv* env)
{
    // SettingsActivity is about to appear
    oldcolors = swapcolors;
    oldundo = allowundo;
    oldhashmem = maxhashmem;
    oldhashinfo = currlayer->showhashinfo;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_SettingsActivity_nativeCloseSettings(JNIEnv* env)
{
    // SettingsActivity is about to disappear
    if (swapcolors != oldcolors) InvertCellColors();

    if (allowundo != oldundo) {
        if (allowundo) {
            if (currlayer->algo->getGeneration() > currlayer->startgen) {
                // undo list is empty but user can Reset, so add a generating change
                // to undo list so user can Undo or Reset (and then Redo if they wish)
                currlayer->undoredo->AddGenChange();
            }
        } else {
            currlayer->undoredo->ClearUndoRedo();
        }
    }

    if (currlayer->showhashinfo != oldhashinfo) {
        // we only show hashing info while generating
        if (generating) lifealgo::setVerbose(currlayer->showhashinfo);
    }

    // need to call setMaxMemory if maxhashmem changed
    if (maxhashmem != oldhashmem) {
        for (int i = 0; i < numlayers; i++) {
            Layer* layer = GetLayer(i);
            if (algoinfo[layer->algtype]->canhash) {
                layer->algo->setMaxMemory(maxhashmem);
            }
            // non-hashing algos (QuickLife) use their default memory setting
        }
    }

    // this is a good place to save the current settings
    SavePrefs();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_SettingsActivity_nativeGetPref(JNIEnv* env, jobject obj, jstring pref)
{
    std::string name = ConvertJString(env, pref);
    if (name == "hash") return currlayer->showhashinfo ? 1 : 0;
    if (name == "time") return showtiming ? 1 : 0;
    if (name == "beep") return allowbeep ? 1 : 0;
    if (name == "swap") return swapcolors ? 1 : 0;
    if (name == "icon") return showicons ? 1 : 0;
    if (name == "undo") return allowundo ? 1 : 0;
    if (name == "grid") return showgridlines ? 1 : 0;
    if (name == "rand") return randomfill;
    if (name == "maxm") return maxhashmem;
    LOGE("Fix bug in nativeGetPref! name = %s", name.c_str());
    return 0;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_SettingsActivity_nativeSetPref(JNIEnv* env, jobject obj, jstring pref, int val)
{
    std::string name = ConvertJString(env, pref);
    if (name == "hash") currlayer->showhashinfo = val == 1; else
    if (name == "time") showtiming = val == 1; else
    if (name == "beep") allowbeep = val == 1; else
    if (name == "swap") swapcolors = val == 1; else
    if (name == "icon") showicons = val == 1; else
    if (name == "undo") allowundo = val == 1; else
    if (name == "grid") showgridlines = val == 1; else
    if (name == "rand") {
        randomfill = val;
        if (randomfill < 1) randomfill = 1;
        if (randomfill > 100) randomfill = 100;
    } else
    if (name == "maxm") {
        maxhashmem = val;
        if (maxhashmem < MIN_MEM_MB) maxhashmem = MIN_MEM_MB;
        if (maxhashmem > MAX_MEM_MB) maxhashmem = MAX_MEM_MB;
    } else
    if (name == "pmode") {
        if (val == 0) SetPasteMode("AND");
        if (val == 1) SetPasteMode("COPY");
        if (val == 2) SetPasteMode("OR");
        if (val == 3) SetPasteMode("XOR");
    } else
        LOGE("Fix bug in nativeSetPref! name = %s", name.c_str());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_SettingsActivity_nativeGetPasteMode(JNIEnv* env)
{
    return env->NewStringUTF(GetPasteMode());
}

// =============================================================================

// these native routines are used in HelpActivity.java:

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_HelpActivity_nativeClassInit(JNIEnv* env, jclass klass)
{
    // get IDs for Java methods in HelpActivity
    help_DownloadFile = env->GetMethodID(klass, "DownloadFile", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_HelpActivity_nativeCreate(JNIEnv* env, jobject obj)
{
    // save obj for calling Java methods in this instance of HelpActivity
    if (helpobj != NULL) env->DeleteGlobalRef(helpobj);
    helpobj = env->NewGlobalRef(obj);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_HelpActivity_nativeDestroy(JNIEnv* env)
{
    // the current instance of HelpActivity is being destroyed
    if (helpobj != NULL) {
        env->DeleteGlobalRef(helpobj);
        helpobj = NULL;
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_HelpActivity_nativeGetURL(JNIEnv* env, jobject obj, jstring jurl, jstring jpageurl)
{
    std::string url = ConvertJString(env, jurl);
    std::string pageurl = ConvertJString(env, jpageurl);

    // for GetURL to work we need to convert any "%20" in pageurl to " "
    size_t start_pos = 0;
    while ((start_pos = pageurl.find("%20", start_pos)) != std::string::npos) {
        pageurl.replace(start_pos, 3, " ");
        start_pos += 1; // skip inserted space
    }

    GetURL(url, pageurl);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_HelpActivity_nativeUnzipFile(JNIEnv* env, jobject obj, jstring jzippath)
{
    std::string zippath = ConvertJString(env, jzippath);
    FixURLPath(zippath);
    std::string entry = zippath.substr(zippath.rfind(':') + 1);
    zippath = zippath.substr(0, zippath.rfind(':'));
    UnzipFile(zippath, entry);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_HelpActivity_nativeDownloadedFile(JNIEnv* env, jobject obj, jstring jurl)
{
    std::string path = ConvertJString(env, jurl);
    path = path.substr(path.rfind('/')+1);
    size_t dotpos = path.rfind('.');
    std::string ext = "";
    if (dotpos != std::string::npos) ext = path.substr(dotpos+1);
    if ( (IsZipFile(path) ||
            strcasecmp(ext.c_str(),"rle") == 0 ||
            strcasecmp(ext.c_str(),"life") == 0 ||
            strcasecmp(ext.c_str(),"mc") == 0)
            // also check for '?' to avoid opening links like ".../detail?name=foo.zip"
            && path.find('?') == std::string::npos) {
        // download file to downloaddir and open it
        path = downloaddir + path;
        std::string url = ConvertJString(env, jurl);
        if (DownloadFile(url, path)) {
            OpenFile(path.c_str());
        }
        return true;
    } else {
        return false;
    }
}

// =============================================================================

// these native routines are used in StateActivity.java:

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_StateActivity_nativeNumStates(JNIEnv* env)
{
    return currlayer->algo->NumCellStates();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jboolean JNICALL Java_net_sf_golly_StateActivity_nativeShowIcons()
{
    return showicons;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_StateActivity_nativeToggleIcons()
{
    showicons = !showicons;
    UpdatePattern();
    UpdateEditBar();
    SavePrefs();
}

// =============================================================================

// these native routines are used in StateGLSurfaceView.java:

static int statewd, stateht;
static int state_offset = 0;
static int max_offset;
static int lastx, lasty;
static bool touch_moved;

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_StateGLSurfaceView_nativeTouchBegan(JNIEnv* env, jobject obj, jint x, jint y)
{
    // LOGI("TouchBegan x=%d y=%d", x, y);
    lastx = x;
    lasty = y;
    touch_moved = false;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jboolean JNICALL Java_net_sf_golly_StateGLSurfaceView_nativeTouchMoved(JNIEnv* env, jobject obj, jint x, jint y)
{
    // LOGI("TouchMoved x=%d y=%d", x, y);
    int boxsize = highdensity ? 64 : 32;
    int oldcol = lastx / boxsize;
    int oldrow = lasty / boxsize;
    int col = x / boxsize;
    int row = y / boxsize;
    lastx = x;
    lasty = y;
    if (col != oldcol || row != oldrow) {
        // user moved finger to a different state box
        touch_moved = true;
    }
    if (max_offset > 0 && row != oldrow) {
        // may need to scroll states by changing state_offset
        int new_offset = state_offset + 10 * (oldrow - row);
        if (new_offset < 0) new_offset = 0;
        if (new_offset > max_offset) new_offset = max_offset;
        if (new_offset != state_offset) {
            // scroll up/down
            state_offset = new_offset;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jboolean JNICALL Java_net_sf_golly_StateGLSurfaceView_nativeTouchEnded(JNIEnv* env)
{
    if (touch_moved) return false;
    if (lastx >= 0 && lastx < statewd && lasty >= 0 && lasty < stateht) {
        // return true if user touched a valid state box
        int boxsize = highdensity ? 64 : 32;
        int col = lastx / boxsize;
        int row = lasty / boxsize;
        int newstate = row * 10 + col + state_offset;
        if (newstate >= 0 && newstate < currlayer->algo->NumCellStates()) {
            currlayer->drawingstate = newstate;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_StateRenderer_nativeInit(JNIEnv* env)
{
    // we only do 2D drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnableClientState(GL_VERTEX_ARRAY);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_StateRenderer_nativeResize(JNIEnv* env, jobject obj, jint w, jint h)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, w, h, 0, -1, 1);     // origin is top left and y increases down
    glViewport(0, 0, w, h);
    glMatrixMode(GL_MODELVIEW);

    statewd = w;
    stateht = h;

    max_offset = 0;
    int numstates = currlayer->algo->NumCellStates();
    if (numstates > 100) {
        max_offset = (((numstates - 100) + 9) / 10) * 10;   // 10 if 101 states, 160 if 256 states
    }
    if (state_offset > max_offset) state_offset = 0;
}

// -----------------------------------------------------------------------------

static void DrawGrid(int wd, int ht)
{
    int cellsize = highdensity ? 64 : 32;
    int h, v;

    // set the stroke color to white
    glColor4ub(255, 255, 255, 255);
    glLineWidth(highdensity ? 2.0 : 1.0);

    v = 1;
    while (v <= ht) {
        GLfloat points[] = {-0.5, v-0.5, wd-0.5, v-0.5};
        glVertexPointer(2, GL_FLOAT, 0, points);
        glDrawArrays(GL_LINES, 0, 2);
        v += cellsize;
    }

    h = 1;
    while (h <= wd) {
        GLfloat points[] = {h-0.5, -0.5, h-0.5, ht-0.5};
        glVertexPointer(2, GL_FLOAT, 0, points);
        glDrawArrays(GL_LINES, 0, 2);
        h += cellsize;
    }
}

// -----------------------------------------------------------------------------

static void DrawRect(int state, int x, int y, int wd, int ht)
{
    GLfloat rect[] = {
        x,    y+ht,  // left, bottom
        x+wd, y+ht,  // right, bottom
        x+wd, y,     // right, top
        x,    y,     // left, top
    };

    glColor4ub(currlayer->cellr[state],
               currlayer->cellg[state],
               currlayer->cellb[state], 255);

    glVertexPointer(2, GL_FLOAT, 0, rect);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// -----------------------------------------------------------------------------

static void DrawIcon(int state, int x, int y)
{
    gBitmapPtr* icons = currlayer->icons31x31;
    if (icons == NULL || icons[state] == NULL) return;
    unsigned char* pxldata = icons[state]->pxldata;
    if (pxldata == NULL) return;

    int cellsize = 31;
    const int maxcoords = 1024;     // must be multiple of 2
    GLfloat points[maxcoords];
    int numcoords = 0;
    bool multicolor = currlayer->multicoloricons;

    glPointSize(highdensity ? 2 : 1);

    unsigned char deadr = currlayer->cellr[0];
    unsigned char deadg = currlayer->cellg[0];
    unsigned char deadb = currlayer->cellb[0];
    unsigned char prevr = deadr;
    unsigned char prevg = deadg;
    unsigned char prevb = deadb;
    glColor4ub(deadr, deadg, deadb, 255);

    // draw non-black pixels in this icon
    unsigned char liver = currlayer->cellr[state];
    unsigned char liveg = currlayer->cellg[state];
    unsigned char liveb = currlayer->cellb[state];
    int byte = 0;
    for (int i = 0; i < cellsize; i++) {
        for (int j = 0; j < cellsize; j++) {
            unsigned char r = pxldata[byte++];
            unsigned char g = pxldata[byte++];
            unsigned char b = pxldata[byte++];
            byte++; // skip alpha
            if (r || g || b) {
                if (multicolor) {
                    // draw non-black pixel from multi-colored icon
                    if (swapcolors) {
                        r = 255 - r;
                        g = 255 - g;
                        b = 255 - b;
                    }
                } else {
                    // grayscale icon
                    if (r == 255) {
                        // replace white pixel with current cell color
                        r = liver;
                        g = liveg;
                        b = liveb;
                    } else {
                        // replace gray pixel with appropriate shade between
                        // live and dead cell colors
                        float frac = (float)r / 255.0;
                        r = (int)(deadr + frac * (liver - deadr) + 0.5);
                        g = (int)(deadg + frac * (liveg - deadg) + 0.5);
                        b = (int)(deadb + frac * (liveb - deadb) + 0.5);
                    }
                }
                // draw r,g,b pixel
                bool changecolor = (r != prevr || g != prevg || b != prevb);
                if (changecolor || numcoords == maxcoords) {
                    if (numcoords > 0) {
                        glVertexPointer(2, GL_FLOAT, 0, points);
                        glDrawArrays(GL_POINTS, 0, numcoords/2);
                        numcoords = 0;
                    }
                    if (changecolor) {
                        prevr = r;
                        prevg = g;
                        prevb = b;
                        glColor4ub(r, g, b, 255);
                    }
                }
                if (highdensity) {
                    points[numcoords++] = x + j*2 + 0.5;
                    points[numcoords++] = y + i*2 + 0.5;
                } else {
                    points[numcoords++] = x + j + 0.5;
                    points[numcoords++] = y + i + 0.5;
                }
            }
        }
    }

    if (numcoords > 0) {
        glVertexPointer(2, GL_FLOAT, 0, points);
        glDrawArrays(GL_POINTS, 0, numcoords/2);
    }
}

// -----------------------------------------------------------------------------

static void DrawDigit(int digit, int x, int y)
{
    unsigned char* pxldata = digits10x10[digit+1]->pxldata;
    int cellsize = 10;
    const int maxcoords = 128;     // must be multiple of 2
    GLfloat points[maxcoords];
    int numcoords = 0;

    glPointSize(highdensity ? 2 : 1);

    unsigned char deadr = currlayer->cellr[0];
    unsigned char deadg = currlayer->cellg[0];
    unsigned char deadb = currlayer->cellb[0];
    unsigned char prevr = deadr;
    unsigned char prevg = deadg;
    unsigned char prevb = deadb;
    glColor4ub(deadr, deadg, deadb, 255);

    // draw non-black pixels in this icon
    int byte = 0;
    for (int i = 0; i < cellsize; i++) {
        for (int j = 0; j < cellsize; j++) {
            unsigned char r = pxldata[byte++];
            unsigned char g = pxldata[byte++];
            unsigned char b = pxldata[byte++];
            byte++; // skip alpha
            if (r || g || b) {
                // draw non-black pixel
                bool changecolor = (r != prevr || g != prevg || b != prevb);
                if (changecolor || numcoords == maxcoords) {
                    if (numcoords > 0) {
                        glVertexPointer(2, GL_FLOAT, 0, points);
                        glDrawArrays(GL_POINTS, 0, numcoords/2);
                        numcoords = 0;
                    }
                    if (changecolor) {
                        prevr = r;
                        prevg = g;
                        prevb = b;
                        glColor4ub(r, g, b, 255);
                    }
                }
                if (highdensity) {
                    points[numcoords++] = x + j*2 + 0.5;
                    points[numcoords++] = y + i*2 + 0.5;
                } else {
                    points[numcoords++] = x + j + 0.5;
                    points[numcoords++] = y + i + 0.5;
                }
            }
        }
    }

    if (numcoords > 0) {
        glVertexPointer(2, GL_FLOAT, 0, points);
        glDrawArrays(GL_POINTS, 0, numcoords/2);
    }
}

// -----------------------------------------------------------------------------

static void DrawStateNumber(int state, int x, int y)
{
    // draw state number in top left corner of each cell
    int digitwd = highdensity ? 12 : 6;
    if (state < 10) {
        // state = 0..9
        DrawDigit(state, x, y);
    } else if (state < 100) {
        // state = 10..99
        DrawDigit(state / 10, x, y);
        DrawDigit(state % 10, x + digitwd, y);
    } else {
        // state = 100..255
        DrawDigit(state / 100, x, y);
        DrawDigit((state % 100) / 10, x + digitwd, y);
        DrawDigit((state % 100) % 10, x + digitwd*2, y);
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_StateRenderer_nativeRender(JNIEnv* env)
{
    // fill the background with state 0 color
    glClearColor(currlayer->cellr[0]/255.0,
                 currlayer->cellg[0]/255.0,
                 currlayer->cellb[0]/255.0,
                 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    DrawGrid(statewd, stateht);

    // display state colors or icons
    int numstates = currlayer->algo->NumCellStates();
    int x = 1;
    int y = 1;
    int first = state_offset;   // might be > 0 if numstates > 100

    if (state_offset == 0) {
        DrawStateNumber(0, 1, 1);
        // start from state 1
        first = 1;
        x = highdensity ? 65 : 33;
    }

    for (int state = first; state < numstates; state++) {
        if (showicons) {
            DrawIcon(state, x, y);
        } else {
            DrawRect(state, x, y, highdensity ? 63 : 31, highdensity ? 63 : 31);
        }
        DrawStateNumber(state, x, y);
        x += highdensity ? 64 : 32;
        if (x > (highdensity ? 640 : 320)) {
            x = 1;
            y += highdensity ? 64 : 32;
            if (y > (highdensity ? 640 : 320)) break;
        }
    }
}

// =============================================================================

// these native routines are used in RuleActivity.java:

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_RuleActivity_nativeSaveCurrentSelection(JNIEnv* env)
{
    SaveCurrentSelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_RuleActivity_nativeGetAlgoIndex(JNIEnv* env)
{
    return currlayer->algtype;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_RuleActivity_nativeGetAlgoName(JNIEnv* env, jobject obj, int algoindex)
{
    if (algoindex < 0 || algoindex >= NumAlgos()) {
        // caller will check for empty string
        return env->NewStringUTF("");
    } else {
        return env->NewStringUTF(GetAlgoName(algoindex));
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_RuleActivity_nativeGetRule(JNIEnv* env)
{
    return env->NewStringUTF(currlayer->algo->getrule());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_RuleActivity_nativeCheckRule(JNIEnv* env, jobject obj, jstring rule, int algoindex)
{
    // if given rule is valid in the given algo then return same rule,
    // otherwise return the algo's default rule
    std::string thisrule = ConvertJString(env, rule);
    if (thisrule.empty()) thisrule = "B3/S23";

    lifealgo* tempalgo = CreateNewUniverse(algoindex);
    const char* err = tempalgo->setrule(thisrule.c_str());
    if (err) {
        // switch to tempalgo's default rule
        std::string defrule = tempalgo->DefaultRule();
        size_t thispos = thisrule.find(':');
        if (thispos != std::string::npos) {
            // preserve valid topology so we can do things like switch from
            // "LifeHistory:T30,20" in RuleLoader to "B3/S23:T30,20" in QuickLife
            size_t defpos = defrule.find(':');
            if (defpos != std::string::npos) {
                // default rule shouldn't have a suffix but play safe and remove it
                defrule = defrule.substr(0, defpos);
            }
            defrule += ":";
            defrule += thisrule.substr(thispos+1);
        }
        thisrule = defrule;
    }
    delete tempalgo;

    return env->NewStringUTF(thisrule.c_str());
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_RuleActivity_nativeCheckAlgo(JNIEnv* env, jobject obj, jstring rule, int algoindex)
{
    std::string thisrule = ConvertJString(env, rule);
    if (thisrule.empty()) thisrule = "B3/S23";

    // 1st check if rule is valid in displayed algo
    lifealgo* tempalgo = CreateNewUniverse(algoindex);
    const char* err = tempalgo->setrule(thisrule.c_str());
    if (err) {
        // check rule in other algos
        for (int newindex = 0; newindex < NumAlgos(); newindex++) {
            if (newindex != algoindex) {
                delete tempalgo;
                tempalgo = CreateNewUniverse(newindex);
                err = tempalgo->setrule(thisrule.c_str());
                if (!err) {
                    delete tempalgo;
                    return newindex;    // tell caller to change algorithm
                }
            }
        }
    }
    delete tempalgo;

    if (err) {
        return -1;          // rule is not valid in any algo
    } else {
        return algoindex;   // no need to change algo
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_RuleActivity_nativeSetRule(JNIEnv* env, jobject obj, jstring rule, int algoindex)
{
    std::string oldrule = currlayer->algo->getrule();
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;

    std::string newrule = ConvertJString(env, rule);
    if (newrule.empty()) newrule = "B3/S23";

    if (algoindex == currlayer->algtype) {
        // check if new rule is valid in current algorithm (if not then revert to oldrule)
        const char* err = currlayer->algo->setrule(newrule.c_str());
        if (err) RestoreRule(oldrule.c_str());

        // convert newrule to canonical form for comparison with oldrule, then
        // check if the rule string changed or if the number of states changed
        newrule = currlayer->algo->getrule();
        int newmaxstate = currlayer->algo->NumCellStates() - 1;
        if (oldrule != newrule || oldmaxstate != newmaxstate) {
            // if grid is bounded then remove any live cells outside grid edges
            if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                ClearOutsideGrid();
            }

            // rule change might have changed the number of cell states;
            // if there are fewer states then pattern might change
            if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
                ReduceCellStates(newmaxstate);
            }

            if (allowundo) {
                currlayer->undoredo->RememberRuleChange(oldrule.c_str());
            }
        }
        UpdateLayerColors();
        UpdateEverything();
    } else {
        // change the current algorithm and switch to the new rule
        // (if the new rule is invalid then the algo's default rule will be used);
        // this also calls UpdateLayerColors, UpdateEverything and RememberAlgoChange
        ChangeAlgorithm(algoindex, newrule.c_str());
    }

    SavePrefs();
}

// =============================================================================

std::string GetRuleName(const std::string& rule)
{
    std::string result = "";
    // not yet implemented!!!
    // maybe we should create rule.h and rule.cpp in gui-common???
    return result;
}

// -----------------------------------------------------------------------------

void UpdateEditBar()
{
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        // this can happen after an algo/rule change
        currlayer->drawingstate = 1;
    }

    // call Java method in MainActivity class to update the buttons in the edit bar
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_UpdateEditBar);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void BeginProgress(const char* title)
{
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jtitle = env->NewStringUTF(title);
        env->CallVoidMethod(mainobj, main_BeginProgress, jtitle);
        env->DeleteLocalRef(jtitle);
    }
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const char* message)
{
    bool result = true;     // abort if getJNIenv fails

    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jint percentage = int(fraction_done * 100);
        jstring jmsg = env->NewStringUTF(message);
        result = env->CallBooleanMethod(mainobj, main_AbortProgress, percentage, jmsg);
        env->DeleteLocalRef(jmsg);
    }
    if (attached) javavm->DetachCurrentThread();

    return result;
}

// -----------------------------------------------------------------------------

void EndProgress()
{
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_EndProgress);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void SwitchToPatternTab()
{
    // switch to MainActivity
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_StartMainActivity);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void ShowTextFile(const char* filepath)
{
    // switch to InfoActivity and display contents of text file
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jpath = env->NewStringUTF(filepath);
        env->CallVoidMethod(mainobj, main_ShowTextFile, jpath);
        env->DeleteLocalRef(jpath);
    }
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void ShowHelp(const char* filepath)
{
    // switch to HelpActivity and display html file
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jpath = env->NewStringUTF(filepath);
        env->CallVoidMethod(mainobj, main_ShowHelp, jpath);
        env->DeleteLocalRef(jpath);
    }
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void AndroidWarning(const char* msg)
{
    if (generating) paused = true;

    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jmsg = env->NewStringUTF(msg);
        env->CallVoidMethod(mainobj, main_Warning, jmsg);
        env->DeleteLocalRef(jmsg);
    }
    if (attached) javavm->DetachCurrentThread();

    if (generating) paused = false;
}

// -----------------------------------------------------------------------------

void AndroidFatal(const char* msg)
{
    paused = true;

    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jmsg = env->NewStringUTF(msg);
        env->CallVoidMethod(mainobj, main_Fatal, jmsg);
        env->DeleteLocalRef(jmsg);
    }
    if (attached) javavm->DetachCurrentThread();

    // main_Fatal calls System.exit(1)
    // exit(1);
}

// -----------------------------------------------------------------------------

bool AndroidYesNo(const char* query)
{
    std::string answer;
    if (generating) paused = true;

    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jquery = env->NewStringUTF(query);
        jstring jresult = (jstring) env->CallObjectMethod(mainobj, main_YesNo, jquery);
        answer = ConvertJString(env, jresult);
        env->DeleteLocalRef(jquery);
        env->DeleteLocalRef(jresult);
    }
    if (attached) javavm->DetachCurrentThread();

    if (generating) paused = false;
    return answer == "yes";
}

// -----------------------------------------------------------------------------

void AndroidBeep()
{
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_PlayBeepSound);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void AndroidRemoveFile(const std::string& filepath)
{
    // LOGI("AndroidRemoveFile: %s", filepath.c_str());
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jpath = env->NewStringUTF(filepath.c_str());
        env->CallVoidMethod(mainobj, main_RemoveFile, jpath);
        env->DeleteLocalRef(jpath);
    }
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

bool AndroidMoveFile(const std::string& inpath, const std::string& outpath)
{
    // LOGE("AndroidMoveFile: %s to %s", inpath.c_str(), outpath.c_str());
    std::string error = "env is null";

    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring joldpath = env->NewStringUTF(inpath.c_str());
        jstring jnewpath = env->NewStringUTF(outpath.c_str());
        jstring jresult = (jstring) env->CallObjectMethod(mainobj, main_MoveFile, joldpath, jnewpath);
        error = ConvertJString(env, jresult);
        env->DeleteLocalRef(joldpath);
        env->DeleteLocalRef(jnewpath);
        env->DeleteLocalRef(jresult);
    }
    if (attached) javavm->DetachCurrentThread();

    return error.length() == 0;
}

// -----------------------------------------------------------------------------

void AndroidFixURLPath(std::string& path)
{
    // replace "%..." with suitable chars for a file path (eg. %20 is changed to space)
    // LOGI("AndroidFixURLPath: %s", path.c_str());

    // no need to do anything???!!!
}

// -----------------------------------------------------------------------------

bool AndroidCopyTextToClipboard(const char* text)
{
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jtext = env->NewStringUTF(text);
        env->CallVoidMethod(mainobj, main_CopyTextToClipboard, jtext);
        env->DeleteLocalRef(jtext);
    }
    if (attached) javavm->DetachCurrentThread();

    return env != NULL;
}

// -----------------------------------------------------------------------------

bool AndroidGetTextFromClipboard(std::string& text)
{
    text = "";
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jtext = (jstring) env->CallObjectMethod(mainobj, main_GetTextFromClipboard);
        text = ConvertJString(env, jtext);
        env->DeleteLocalRef(jtext);
    }
    if (attached) javavm->DetachCurrentThread();

    if (text.length() == 0) {
        ErrorMessage("No text in clipboard.");
        return false;
    } else {
        return true;
    }
}

// -----------------------------------------------------------------------------

void AndroidCheckEvents()
{
    // event_checker is > 0 in here (see gui-common/utils.cpp)
    if (rendering) {
        // best not to call CheckMessageQueue while DrawPattern is executing
        // (speeds up generating loop and might help avoid fatal SIGSEGV error)
        return;
    }
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, main_CheckMessageQueue);
    if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

bool AndroidDownloadFile(const std::string& url, const std::string& filepath)
{
    // LOGI("AndroidDownloadFile: url=%s file=%s", url.c_str(), filepath.c_str());
    std::string error = "env is null";

    // call DownloadFile method in HelpActivity
    bool attached;
    JNIEnv* env = getJNIenv(&attached);
    if (env) {
        jstring jurl = env->NewStringUTF(url.c_str());
        jstring jfilepath = env->NewStringUTF(filepath.c_str());
        jstring jresult = (jstring) env->CallObjectMethod(helpobj, help_DownloadFile, jurl, jfilepath);
        error = ConvertJString(env, jresult);
        env->DeleteLocalRef(jurl);
        env->DeleteLocalRef(jfilepath);
        env->DeleteLocalRef(jresult);
    }
    if (attached) javavm->DetachCurrentThread();

    return error.length() == 0;
}
