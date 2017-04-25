/*
 * Sensory Confidential
 * Copyright (C)2000-2015 Sensory Inc.
 *
 *---------------------------------------------------------------------------
 * Sensory TrulyHandsfree SDK Example: recogEnroll.c
 * This example shows demonstrates the enrollment and testing phases of 
 * enrolled fixed triggers (EFT) and user-defined triggers (UDT).
 *
 * NOTE: The same basic steps are followed to create both an enrolled fixed trigger
 * and user-defined trigger. The trigger type is determined by the specific 
 * task file used during initialization.
 *
 *---------------------------------------------------------------------------
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <trulyhandsfree.h>

#ifdef __ANDROID__
# include "common.h"
# define dispn(cons, text) disp(inst, job, text)
# define DISP(cons, text) disp(inst, job, text)
# define PATH(a) strdup(strcat(strcpy(tmpFileName, appDir), a))
# define START_REC() startRecord(inst, jrecord)
# define STOP_REC()  stopRecord(inst, jrecord)
# define GET_NEXT_BUFFER() audioGetNextBuffer(inst, jrecord)
#else
# include "console.h"
# include "audio.h"
# include "datalocations.h"  /* Specifies which language pack to use */
# define DISP(cons, text) disp(cons, text)
# ifdef WIN32
#  define PATH(a) _strdup(a)
# else
#  define PATH(a) strdup(a)
#endif
# define START_REC() startRecord()
# define STOP_REC()  stopRecord()
# define GET_NEXT_BUFFER() audioGetNextBuffer(&done)
#endif

/* Use the following macro to select between enroll fixed trigger (EFT) and user-defined trigger (UDT) tasks */
#define TASK_UDT      (1)  /* Set to 0 for EFT or 1 for UDT */
#if TASK_UDT
# define TASK_FILE    "UDT_enUS_taskData_v12_0_5.raw"  /* UDT Task file */
# define PHRASE       "<any passphrase>"
#else
# define TASK_FILE    "HBG_enUS_taskData_v2_2.raw"    /* EFT Task file for the phrase "Hello Blue Genie" */
# define PHRASE       "Hello Blue Genie"
#endif

/* Use the following macro to control whether to load data from the enrollment cache (if available). */
/* The enrollment cache allows for modifying the enrollment set (e.g., adding additional users) at a later time */
#define USE_CACHE     (0)  /* Set to 0 to ignore cache or 1 to load cache (if exists) */
#define CACHE_FILE    "enrollCache.raw"     

#define ENROLLED_NETFILE     "enrollNet.raw"    /* Output: combined trigger acoustic model for all users */
#define ENROLLED_SEARCHFILE  "enrollSearch.raw" /* Output: combined trigger search for all users */
#define EMBEDDED_NETFILE     "enrollNet.bin"    /* Output: combined trigger recognizer for all users */
#define EMBEDDED_SEARCHFILE  "enrollSearch.bin" /* Output: combined trigger search for all users */


#define EMBEDDED_TARGET  "pc38"                /* Target used when saving embedded data files */

#define NUM_USERS          (2)
#define NUM_ENROLL         (3)
#define NUM_TEST           (5)
#define NBEST              (1)             /* Number of results  */
#define MAXSTR             (512)           /* Output string size */
#define SDET_LSILENCE      (60000.f)
#define SDET_TSILENCE      (500.f)
#define SDET_MAXRECORD     (60000.f)
#define SDET_MINDURATION   (500.f)

#define THROW(a) { ewhere=(a); goto error; }
#define THROW2(a,b) {ewhere=(a); ewhat=(b); goto error; }


/*------------------------------------------------------------------------------*/
/* Define a small value so that printf() yields the same output with
   different compilers; MS always rounds up, gcc (?) uses "banker's rounding" */
#define EPSILON 1.0e-5

void printValue(void *cons, void *inst, void *job,
                int rIdx, const char *valName,
                thfEnrollmentCheckOne_t *checkZero,  thfEnrollmentCheckOne_t *checkThis)
{
  char *nomark = "  ";
  char *mark = "**";
  char str[MAXSTR];
  sprintf(str, "%-11s %s %-8.2f (%.2f)", valName, 
    checkThis->pass[rIdx] ? nomark : mark, 
    checkThis->score[rIdx] + EPSILON,
    checkZero->thresh[rIdx] + EPSILON);
  DISP(cons, str);
}

void printValues(void * cons, void *inst, void *job, 
                 thfEnrollmentCheck_t *enrollmentCheckResult, int eIdx)
{
  char str[MAXSTR];
  thfEnrollmentCheckOne_t *checkZero = &(enrollmentCheckResult->enroll[0]);
  thfEnrollmentCheckOne_t *checkThis = &(enrollmentCheckResult->enroll[eIdx]);

  sprintf(str, "Check File %d: ", eIdx);
  DISP(cons, str);
  sprintf(str, "%-13s  %-8s %-11s", "Attribute", "Value", "(Threshold)");
  DISP(cons, str);
  printValue(cons, inst, job, CHECK_TRIGGER_ENERGY_MIN, "ENERGY_MIN", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_ENERGY_STD_DEV, "ENERGY_VAR", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_SIL_BEG_MSEC, "SIL_BEG(ms)", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_SIL_END_MSEC, "SIL_END(ms)", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_SNR, "SNR", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_RECORDING_VARIANCE, "REC VAR", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_POOR_RECORDINGS_LIMIT, "POOR REC", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_CLIPPING_FRAMES, "CLIP (fr)", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_VOWEL_DUR, "VOWEL_DUR", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_REPETITION, "REPETITION", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_SIL_IN_PHRASE, "SILENCE", checkZero, checkThis);
  printValue(cons, inst, job, CHECK_TRIGGER_SPOT, "SPOT", checkZero, checkThis);
}

void printEnrollFeedback(void *cons, void *inst, void *job, 
          thfEnrollmentCheck_t *enrollmentCheckResult) {
  int  eIdx;
  DISP(cons, "");
  for (eIdx = 0; eIdx < enrollmentCheckResult->numEnroll; eIdx++) {
    printValues(cons, inst, job, enrollmentCheckResult, eIdx);
    DISP(cons, "");
  }
  DISP(cons, "");
  return;
}


/* ------------------------------------------------------------------------------ */
void printTaskInfo(void *cons, void *inst, void* job, thf_t *ses, adaptObj_t *aObj, char *taskName) {
  thfAdaptTaskInfo_t *taskInfo = NULL;
  char str[MAXSTR];
  thfAdaptTaskInfo(ses, aObj, taskName, &taskInfo);
  if(taskInfo) {
    DISP(cons,"Task Parameter Settings:");
    sprintf(str, "  UDT            \t: %d", taskInfo->userDefinedTrigger); 
    DISP(cons, str);
    sprintf(str, "  featureType    \t: %d", taskInfo->featureType); 
    DISP(cons, str);
    sprintf(str, "  sampleRate     \t: %d", taskInfo->sampleRate); 
    DISP(cons, str);
    sprintf(str, "  frameSize      \t: %d", taskInfo->frameSize); 
    DISP(cons, str);
    sprintf(str, "  delay1         \t: %d", taskInfo->delay1); 
    DISP(cons, str);
    sprintf(str, "  delayM         \t: %d", taskInfo->delayM); 
    DISP(cons, str);
    sprintf(str, "  epqMin         \t: %.2f", taskInfo->epqMin); 
    DISP(cons, str);
    sprintf(str, "  svThresh       \t: %.2f", taskInfo->svThresh); 
    DISP(cons, str);
    sprintf(str, "  paramAoffset   \t: %d", taskInfo->paramAoffset); 
    DISP(cons, str);
    sprintf(str, "  doSpeechDetect \t: %d", taskInfo->doSpeechDetect); 
    DISP(cons, str);
    sprintf(str, "  checkPhraseQuality \t: %.2f", taskInfo->checkPhraseQuality); 
    DISP(cons, str);
    sprintf(str, "  checkEnergyMin     \t: %.2f", taskInfo->checkEnergyMin); 
    DISP(cons, str);
    sprintf(str, "  checkEnergyStdDev  \t: %.2f", taskInfo->checkEnergyStdDev); 
    DISP(cons, str);
    sprintf(str, "  checkSilBegMsec    \t: %.2f", taskInfo->checkSilBegMsec); 
    DISP(cons, str);
    sprintf(str, "  checkSilEndMsec    \t: %.2f", taskInfo->checkSilEndMsec); 
    DISP(cons, str);
    sprintf(str, "  checkSNR           \t: %.2f", taskInfo->checkSNR); 
    DISP(cons, str);
    sprintf(str, "  checkRecordingVariance: %.2f", taskInfo->checkRecordingVariance); 
    DISP(cons, str);
    sprintf(str, "  checkPoorRecordLimit  : %.2f", taskInfo->checkPoorRecordingsLimit); 
    DISP(cons, str);
    sprintf(str, "  checkClippingFrames\t: %.2f", taskInfo->checkClippingFrames); 
    DISP(cons, str);
    sprintf(str, "  checkVowelDur      \t: %.2f", taskInfo->checkVowelDur); 
    DISP(cons, str);
    sprintf(str, "  checkRepetition    \t: %.2f", taskInfo->checkRepetition); 
    DISP(cons, str);
    sprintf(str, "  checkSilInPhrase   \t: %.2f", taskInfo->checkSilInPhrase); 
    DISP(cons, str);
    sprintf(str, "  checkSpot          \t: %.2f", taskInfo->checkSpot); 
    DISP(cons, str);
    thfAdaptTaskInfoDestroy(ses, aObj, taskName, taskInfo);
    DISP(cons, "");
  }
}


/*------------------------------------------------------------------------------*/
void printSdetStatus(void *cons, void *inst, void *job, int status) {
  switch (status) {
  case RECOG_QUIET:
    DISP(cons, "quiet");
    break;
  case RECOG_SOUND:
    DISP(cons, "sound");
    break;
  case RECOG_IGNORE:
    DISP(cons, "ignored");
    break;
  case RECOG_SILENCE:
    DISP(cons, "timeout: silence");
    break;
  case RECOG_MAXREC:
    DISP(cons, "timeout: maxrecord");
    break;
  case RECOG_DONE:
    DISP(cons, "end-of-speech");
    break;
  default:
    DISP(cons, "other");
    break;
  }
}


/*------------------------------------------------------------------------------*/

#if defined(__iOS__)
int enroll(void *inst)
# define job  NULL
#elif defined(__ANDROID__)
jlong Java_com_sensory_TrulyHandsfreeSDK_Console_recogEnroll(JNIEnv* inst,
		jobject job, jstring jappDir, jobject jrecord)
#else
int main(int argc, char **argv)
# define inst NULL
# define job  NULL
#endif
{
  int res=0;
  int fail=0;
  const char *ewhere=NULL, *ewhat = NULL;
  thf_t *ses = NULL;  
  adaptObj_t *aObj = NULL;
  recog_t *enrollRecog = NULL;  	 /* Used for speech detection during enrollment */
  recog_t *recogTrigger = NULL;    /* Used for testing enrolled trigger */
  searchs_t *searchTrigger = NULL; /* used for testing enrolled trigger */
  const char *rres = NULL;
  char str[MAXSTR];
  unsigned short status, done = 0;
  audiol_t *aud = NULL;
  void *cons = NULL;
  float lsil = SDET_LSILENCE, tsil = SDET_TSILENCE, maxR = SDET_MAXRECORD, minDur = SDET_MINDURATION;
  unsigned short numUsers = NUM_USERS;
  unsigned short numEnroll = NUM_ENROLL;
  unsigned short eIdx, uIdx=0;
  thfAdaptUserInfo_t *userInfo = NULL;
  thfModelTrigger_t *modelTrigger = NULL;
  int qualityStrIdx = 0;
  char *qualityDescription[] = {"VERY POOR", "POOR", "FAIR", "GOOD"};
  char *taskType="trigger";
  char *taskName="trigger";
  float svThresh; // This phrasespot param gets returned by enrollment
  int i;
#ifdef __ANDROID__
  const char *appDir = (*inst)->GetStringUTFChars(inst, jappDir, 0);
  char tmpFileName[MAXSTR];
#endif
  thfEnrollmentCheck_t *enrollmentCheckResult = NULL;


  char *cacheFile=PATH(SAMPLES_DATADIR CACHE_FILE);
  char *netFile=PATH(THF_DATADIR NETFILE);
  char *enrolledNetFile=PATH(SAMPLES_DATADIR ENROLLED_NETFILE);
  char *enrolledSearchFile=PATH(SAMPLES_DATADIR ENROLLED_SEARCHFILE);
  char *embeddedNetFile=PATH(SAMPLES_DATADIR EMBEDDED_NETFILE);
  char *embeddedSearchFile=PATH(SAMPLES_DATADIR EMBEDDED_SEARCHFILE);
  char *taskFile=NULL;
#if TASK_UDT
  taskFile=PATH(THF_LVCSR_DATADIR TASK_FILE);
#else
  taskFile=PATH(SAMPLES_DATADIR TASK_FILE);
#endif

  /* Draw console */
  if (!(cons = initConsole(inst) ))
    return 1;
  
  /* Create SDK session */
  if (!(ses = thfSessionCreate())) {
    panic(cons, "thfSessionCreate", thfGetLastError(NULL), 0);
    return 1;
  }
#ifdef __ANDROID__
  displaySdkVersionAndExpirationDate(inst, job);
#endif
  
  /* Initialize adapt task */
  if(!(aObj=thfAdaptCreate(ses)))
    THROW("thfAdaptCreate");
  if(!thfAdaptTaskCreate(ses, aObj, taskType,taskFile , taskName))
    THROW("thfAdaptTaskCreate");
  printTaskInfo(cons, inst, job, ses, aObj, taskName);
  
  /* Optional:  Save the enrolled audio (useful for transferring as a package perhaps) */
  if (!thfAdaptTaskSet(ses, aObj, taskName, "ADAPT_SAVE_ENROLLMENT_AUDIO", 1.0))
    THROW("thfAdaptTaskSet");

  /* Optional: load cache file (if exists) */
  if(USE_CACHE) {
    FILE *fp = fopen(cacheFile, "rb");
    if (fp) {
      thfAdaptTaskInfo_t *taskInfo = NULL;
      thfAdaptTaskInfo(ses, aObj, taskName, &taskInfo);
      fclose(fp);
      sprintf(str, "Loading cache file %s:", cacheFile);
      DISP(cons, str);
      if(!thfAdaptUserReadFromFile(ses, aObj, taskName, cacheFile))
	      THROW("thfAdaptUserReadFromFile");
      if(!thfAdaptUserInfo(ses, aObj, taskName, &userInfo))
	      THROW("thfAdaptUserInfo");
      for (uIdx = 0; uIdx < userInfo->numUsers; uIdx++) {
	sprintf(str, "\tPhraseID '%s' has %d enrollments", 
		userInfo->user[uIdx].name, userInfo->user[uIdx].numEnroll);
	  DISP(cons, str);
        for (eIdx = 0; eIdx < userInfo->user[uIdx].numEnroll; eIdx++) {
          sprintf(str, "  Audio %d length %.3f", eIdx + 1,
            userInfo->user[uIdx].enrollInfo[eIdx].audioLen / (double)taskInfo->sampleRate);
          DISP(cons, str);
        }
        DISP(cons, "");
      }
      if(!thfAdaptUserInfoDestroy(ses, userInfo))
	      THROW("thfAdaptUserInfoDestroy");
      thfAdaptTaskInfoDestroy(ses, aObj, taskName, taskInfo);
    }
  }
  
  /* Recognizer - used for speech detection of enrollment phrases */
  if (!(enrollRecog = thfRecogCreateFromFile(ses, netFile, 0xFFFF, -1, SDET)))
    THROW("thfRecogCreateFromFile");
  if (!thfRecogConfigSet(ses, enrollRecog, REC_LSILENCE, lsil))
    THROW("thfRecogConfigSet: lsil");
  if (!thfRecogConfigSet(ses, enrollRecog, REC_TSILENCE, tsil))
    THROW("thfRecogConfigSet: tsil");
  if (!thfRecogConfigSet(ses, enrollRecog, REC_MAXREC, maxR))
    THROW("thfRecogConfigSet: maxrec");
  if (!thfRecogConfigSet(ses, enrollRecog, REC_MINDUR, minDur))
    THROW("thfRecogConfigSet: mindur");	    
  if (!thfRecogInit(ses, enrollRecog, NULL, RECOG_KEEP_WAVE))
    THROW("thfRecogInit");
  
  
  /* Initialize audio */
  DISP(cons, "Initializing audio...");
  initAudio(inst, cons);
  STOP_REC(); /* initAudio() starts recording immediately, which we don't want right now */
  
  /* --------- get all enrollment recordings from each user ---------*/
  
  /* uIdx may be non-null was cache file successfully loaded */
  for (; uIdx < numUsers; uIdx++) {
    char phraseID[MAXSTR];
    sprintf(phraseID, "user-%i", uIdx+1);
    sprintf(str, "\n===== %s =====\n", phraseID);
    
    /* add user to adaptation task */
    if(!thfAdaptUserAdd(ses, aObj, taskName, phraseID))
      THROW("thfAdaptUserAdd");
    
    /* Record enrollment phrases for each user */
    for (eIdx = 0; eIdx < numEnroll; eIdx++) {
      sprintf(str, "\n===== %s: Enroll %d of %d =====\n", phraseID, eIdx + 1, numEnroll);
      DISP(cons, str);
      
      /* Pipelined recording */
      if (START_REC() == RECOG_NODATA) break;
      done = 0;
      rres = NULL;
      sprintf(str, "Say: " PHRASE);
      DISP(cons, str);
      while (!done) { /* Feeds audio into recognizer until speech detector returns done */
	audioEventLoop();
	while ((aud = GET_NEXT_BUFFER()) && !done) {
	  if (!thfRecogPipe(ses, enrollRecog, aud->len, aud->audio, SDET_ONLY, &status))
	    THROW("recogPipe");
	  printSdetStatus(cons,inst,job,status);
	  audioNukeBuffer(aud);
	  aud = NULL;
	  if (status == RECOG_DONE) {
	    done = 1;
	    STOP_REC();
	  }
	}
	if (aud)audioNukeBuffer(aud);
      }
      
      /* Retrieve speech detected region and add to enrollment */ 
      if (status == RECOG_DONE) {
	thfTriggerEnrollment_t *triggerEnrollment = malloc(sizeof(thfTriggerEnrollment_t));
	const short *sdetSpeech;
	unsigned long sdetSpeechLen;
	float score = 0;
	if (!thfRecogResult(ses, enrollRecog, &score, &rres, NULL,
			    NULL, NULL, NULL, &sdetSpeech, &sdetSpeechLen))
	  THROW("thfRecogResult");
	triggerEnrollment->sampFreq     = 16000;
	triggerEnrollment->audio        = (short*)sdetSpeech;
	triggerEnrollment->audioLen     = sdetSpeechLen;
	triggerEnrollment->score        = 0;
	triggerEnrollment->time         = time(NULL);
	triggerEnrollment->enrollmentID = -1; // this gets updated internally
	triggerEnrollment->reserved1    = NULL;
	triggerEnrollment->reserved2    = NULL;
	triggerEnrollment->transcription = NULL;
	triggerEnrollment->context      = NULL;
	if(!thfAdaptEnrollmentAddTrigger(ses, aObj, taskName, phraseID, triggerEnrollment))
	  THROW("thfAdaptEnrollmentAddTrigger");
	free(triggerEnrollment);
      }
      thfRecogReset(ses, enrollRecog);
      
    }  /* end of numEnroll loop */
    
    
    /* --------------------------------------------------------------- */
    /* Check enrollments and print feedback for the current user  */
    DISP(cons, "\n----------");
    sprintf(str, "Checking enrollments for phraseID: %s", phraseID);
    DISP(cons, str);
    if(!thfAdaptEnrollmentCheck(ses, aObj, taskName, phraseID, &enrollmentCheckResult))
      THROW("thfAdaptEnrollmentCheck");
    if (enrollmentCheckResult->quality < 0.35) qualityStrIdx = 0;
    else if (enrollmentCheckResult->quality < 0.5) qualityStrIdx = 1;
    else if (enrollmentCheckResult->quality < 0.7) qualityStrIdx = 2;
    else qualityStrIdx = 3;
    
    sprintf(str,"Phrase quality: %.2f (%s; thresh=%.2f)\n", enrollmentCheckResult->quality,
	    qualityDescription[qualityStrIdx], enrollmentCheckResult->qualityThresh);
    DISP(cons, str);

    printEnrollFeedback(cons, inst, job, enrollmentCheckResult);
    if (enrollmentCheckResult->allPass) {
      sprintf(str,"\tNo detected problems in enrollment recordings for phraseID: %s\n", phraseID);
      DISP(cons, str);
    } else {
      sprintf(str,"\tWARNING: Problems detected in enrollment recordings for phraseID: %s (see '**' marks)\n", phraseID);
      DISP(cons, str);   
      fail=1;
    }

    if (!thfAdaptEnrollmentCheckDestroy(ses, enrollmentCheckResult))
      THROW("thfAdaptEnrollmentCheckDestroy");
    enrollmentCheckResult = NULL;
    
  } /* end of numUsers loop */
  

  /* Enforce strict enrollment checks. Only proceed with enrollment if ALL checks pass */
  if(fail) {
    DISP(cons,"!!!!!!!!!!!!!!!!");
    DISP(cons,"Some enrollment checks failed (see above).\nPlease resolve issues and re-enroll.\n");
    DISP(cons,"!!!!!!!!!!!!!!!!");
  } else {
    
    /* ---------------------------------------------------------------- */
    /* Perform adaptation to create an enrolled trigger  */
    DISP(cons,"Adapting...\n");
    if (!thfAdaptModelAdapt(ses, aObj, taskName, NULL, 1.0))
      THROW("thfAdaptModelAdapt");
    
    /* ---------------------------------------------------------------- */
    /* Retrieve enrolled trigger and save to file */
    
    if (!thfAdaptModelGetTrigger(ses, aObj, taskName, &modelTrigger))
      THROW("thfAdaptModelGetTrigger");
    
    if (!thfAdaptModelSaveTriggerToFile(ses, aObj, modelTrigger, enrolledSearchFile, enrolledNetFile))
      THROW("thfAdaptModelSaveTriggerToFile");
    
    /* Optional: save enrolled trigger in deeply embedded format */
    if (!thfSaveEmbedded(ses, modelTrigger->recog, modelTrigger->search, EMBEDDED_TARGET,
			 embeddedNetFile, embeddedSearchFile, 0, 1))
      THROW("thfSaveEmbedded");  
    
    /* Optional: save cache file so additional enrollments can be made at a later time */
    sprintf(str, "Saving cache file '%s'\n", cacheFile);
    DISP(cons, str);
    if (!thfAdaptUserSaveToFile(ses, aObj, taskName, NULL, cacheFile))
      THROW("thfAdaptUserSaveToFile");
    
    // Extract the recommended phrasespotting params 
    // NOTE: psDelay and epqMin are also available but are already configured in the enrolled search
    svThresh=modelTrigger->spotParams[TRIGGER_ENROLL_SV_THRESH];
    
    if (!thfAdaptModelDestroyTrigger(ses, modelTrigger, 0, 0))
      THROW("thfAdaptModelDestroyTrigger");
    
    thfRecogDestroy(enrollRecog); enrollRecog=NULL;
    
    /* --------- Test phase ---------*/
    
    /* Load enrolled trigger (ie, recog and search) from file */ 
    /* NOTE: This step is included for completeness. Technically, they could be used directly from the 
       modelTrigger data structure following enrollment */
    if (!(recogTrigger = thfRecogCreateFromFile(ses, enrolledNetFile, 0xFFFF, -1, NO_SDET)))
      THROW("thfRecogCreateFromFile");
    if(!(searchTrigger = thfSearchCreateFromFile(ses, recogTrigger,	enrolledSearchFile, NBEST)))
      THROW("thfSearchCreateFromFile");
    
    if (!thfRecogInit(ses, recogTrigger, searchTrigger, RECOG_KEEP_WAVE))
      THROW("thfRecogInit");
    
    DISP(cons, "Testing...");
    /* Record enroll phrases for each user */
    for (i = 0; i < NUM_TEST; i++) {
      sprintf(str, "\n===== Test %d of %d =====\n", i + 1, NUM_TEST);
      DISP(cons, str);
      
      /* Pipelined recognition */
      if (START_REC() == RECOG_NODATA)
	break;
      done = 0;
      rres = NULL;
      sprintf(str, "Say: " PHRASE);
      DISP(cons, str);
      while (!done) { /* Feeds audio into recognizer until sdet done */
	audioEventLoop();
	while ((aud = GET_NEXT_BUFFER()) && !done) {
	  if (!thfRecogPipe(ses, recogTrigger, aud->len, aud->audio, RECOG_ONLY, &status))
	    THROW("recogPipe");
	  printSdetStatus(cons, inst, job, status);
	  audioNukeBuffer(aud); aud = NULL;
	  if (status == RECOG_DONE) {
	    done = 1;
	    STOP_REC();
	  }
	}
	if (aud) audioNukeBuffer(aud);
      }
      /* Process recognition result */
      if (status == RECOG_DONE) {
	float score = 0;
	float svScore;
	if (!thfRecogResult(ses, recogTrigger, &score, &rres, NULL, NULL,
			    NULL, NULL, NULL, NULL ))
	  THROW("thfRecogResult");
	if (rres) {
	  sprintf(str, "Result: %s", rres);
	  DISP(cons, str);
	  svScore = thfRecogSVscore(ses, recogTrigger);
	  sprintf(str, "SV score: %f (svThresh=%f)", svScore, svThresh);
	  DISP(cons, str);	
	  if (svScore >= svThresh) {
	    sprintf(str, "ACCEPTED %s\n", rres);
	    DISP(cons, str);
	  } else {
	  sprintf(str, "REJECTED %s\n", rres);
	  DISP(cons, str);
	  }
	} else {
	  DISP(cons, "No recognition result");
	}
      }
      thfRecogReset(ses, recogTrigger);
    }
  }
  
  /* ---------------------------------------------------------------- */
 error: 
  DISP(cons, "Cleaning up...");
  killAudio();
  free(taskFile);
  free(cacheFile);
  free(netFile);
  free(enrolledNetFile);
  free(enrolledSearchFile);
  free(embeddedNetFile);
  free(embeddedSearchFile);
  if(enrollRecog)thfRecogDestroy(enrollRecog);
  if(recogTrigger)thfRecogDestroy(recogTrigger);
  if(searchTrigger)thfSearchDestroy(searchTrigger);
  if (!thfAdaptDestroy(ses, aObj))
    THROW("thfAdaptDestroy");
  if(ewhere) {
    if (!ewhat && ses) ewhat = thfGetLastError(ses);
    sprintf(str, "\nERROR: %s: %s", ewhere,ewhat); DISP(cons, str);
    res=1;
  } else {
    DISP(cons, "Done");
    closeConsole(cons);
  }
  thfSessionDestroy(ses);
  return res;
}
