/*
 * Sensory Confidential
 *
 * Copyright (C)2000-2015 Sensory Inc.
 *
 *---------------------------------------------------------------------------
 * Sensory TrulyHandsfree SDK Example: PhraseSpot.c
 *---------------------------------------------------------------------------
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <trulyhandsfree.h>

#include <console.h>
#include <audio.h>
#include <datalocations.h>

#define HBG_GENDER_MODEL "hbg_genderModel.raw"
#define HBG_NETFILE      "sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_am.raw"
#define HBG_SEARCHFILE   "sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_9.raw" // pre-built search
#define HBG_OUTFILE      "myhbg.raw"           /* hbg output search */

#define NBEST              (1)                /* Number of results */
#define PHRASESPOT_PARAMA_OFFSET   (0)        /* Phrasespotting ParamA Offset */
#define PHRASESPOT_BEAM    (100.f)            /* Pruning beam */
#define PHRASESPOT_ABSBEAM (100.f)            /* Pruning absolute beam */
#define PHRASESPOT_DELAY  15                 /* Phrasespotting Delay */
#define MAXSTR             (512)              /* Output string size */

#define THROW(a) { ewhere=(a); goto error; }
#define THROW2(a,b) {ewhere=(a); ewhat=(b); goto error; }


char *formatExpirationDate(time_t expiration)
{
  static char expdate[33];
  if (!expiration) return "never";
  strftime(expdate, 32, "%m/%d/%Y 00:00:00 GMT", gmtime(&expiration));
  return expdate;
}

#define inst NULL
int main(int argc, char **argv)
{
  const char *rres;
  thf_t *ses=NULL;
  recog_t *r=NULL;
  searchs_t *s=NULL;
  pronuns_t *pp=NULL;
  char str[MAXSTR];
  const char *ewhere, *ewhat=NULL;
  float score, delay;
  unsigned short status, done=0;
  audiol_t *p;
  unsigned short i=0;
  void *cons=NULL;
  FILE *fp=NULL;

  int j=0;
  unsigned short phraseCount = 2;
  const char *p0="Hello Blue Genie", *p1="This is a test";
  const char *phrase[] = {p0,p1};


  /* Draw console */
  if (!(cons=initConsole(inst))) return 1;

  /* Create SDK session */
  if(!(ses=thfSessionCreate())) {
    panic(cons, "thfSessionCreate", thfGetLastError(NULL), 0);
    return 1;
  }

  /* Display TrulyHandsfree SDK library version number */
  dispv(cons, "TrulyHandsfree SDK Library version: %s\n", thfVersion());
  dispv(cons, "Expiration date: %s\n\n",
	formatExpirationDate(thfGetLicenseExpiration()));

  /* Create recognizer */
  /* NOTE: HBG_NETFILE is a custom model for 'hello blue genie'. Use a generic acoustic model if you change vocabulary or contact Sensory for assistance in developing a custom acoustic model specific to your vocabulary. */
  disp(cons,"Loading recognizer: "  HBG_NETFILE);
  if(!(r=thfRecogCreateFromFile(ses, SAMPLES_DATADIR HBG_NETFILE,(unsigned short)(AUDIO_BUFFERSZ/1000.f*SAMPLERATE),-1,NO_SDET))) 
    THROW("thfRecogCreateFromFile");


  disp(cons,"Loading custom HBG search: " SAMPLES_DATADIR HBG_SEARCHFILE);
  if(!(s=thfSearchCreateFromFile(ses,r,SAMPLES_DATADIR HBG_SEARCHFILE,NBEST)))
    THROW("thfSearchCreateFromFile");

  /* Configure parameters - only DELAY... others are saved in search already */
  disp(cons,"Setting parameters...");  
  delay=PHRASESPOT_DELAY;
  if (!thfPhrasespotConfigSet(ses,r,s,PS_DELAY,delay))
    THROW("thfPhrasespotConfigSet: delay");

  if (thfRecogGetSampleRate(ses,r) != SAMPLERATE)
    THROW("Acoustic model is incompatible with audio samplerate");


  /* Initialize recognizer */
  disp(cons,"Initializing recognizer...");

  if(!thfRecogInit(ses,r,s,RECOG_KEEP_WAVE_WORD_PHONEME))
    THROW("thfRecogInit");

  /* initialize a speaker object with a single speaker,
   * and arbitrarily set it up
   * to hold one recording from this speaker
   */
  if (!thfSpeakerInit(ses,r,0,1)) {
    THROW("thfSpeakerInit");
  }
  
  disp(cons,"Loading gender model: " HBG_GENDER_MODEL);
  /* now read the gender model, which has been tuned to the target phrase */
  if (!thfSpeakerReadGenderModel(ses,r,SAMPLES_DATADIR HBG_GENDER_MODEL)) {
    THROW("thfSpeakerReadGenderModel");
  }

  /* Initialize audio */
  disp(cons,"Initializing audio...");
  initAudio(inst,cons);

  for (i=1; i<=5; i++) {
    sprintf(str,"\n===== LOOP %i of 5 ======",i); disp(cons,str);
    disp(cons,"Recording...");
    if (startRecord() == RECOG_NODATA)
      break;

    disp(cons,">>> Say 'Hello Blue Genie' <<<");

    /* Pipelined recognition */
    done=0;
    while (!done) {
      audioEventLoop();
      while ((p = audioGetNextBuffer(&done)) && !done) {
        if (!thfRecogPipe(ses, r, p->len, p->audio, RECOG_ONLY, &status))
          THROW("recogPipe");
        audioNukeBuffer(p);
        if (status == RECOG_DONE) {
          done = 1;
          stopRecord();
        }
      }
    }
    
    /* Report N-best recognition result */
    rres=NULL;
    if (status==RECOG_DONE) {
      disp(cons,"Recognition results...");
      score=0;
      if (!thfRecogResult(ses,r,&score,&rres,NULL,NULL,NULL,NULL,NULL,NULL)) 
        THROW("thfRecogResult");
      sprintf(str,"Result: %s (%.3f)",rres,score); disp(cons,str);

      {
        float genderProb;
        if (!thfSpeakerGender(ses, r, &genderProb)) {
          THROW("thfSpeakerGender");
        }
        if (genderProb < 0.5) {
          sprintf(str, "Gender: MALE (score=%f)\n", (1.0 - genderProb));
        }
        else {
          sprintf(str, "Gender: FEMALE (score=%f)\n", genderProb);
        }
        disp(cons, str);
      }

    } else 
      disp(cons,"No recognition result");
    thfRecogReset(ses,r);
  }
  /* Clean up */
  killAudio();
  thfRecogDestroy(r); r=NULL;
  thfSearchDestroy(s); s=NULL;
  thfPronunDestroy(pp); pp=NULL;
  thfSessionDestroy(ses); ses=NULL;
  disp(cons,"Done");
  closeConsole(cons);

  return 0;

  /* Async error handling, see console.h for panic */
 error:
  killAudio();
  if(!ewhat && ses) ewhat=thfGetLastError(ses);
  if(fp) fclose(fp);
  if(r) thfRecogDestroy(r);
  if(s) thfSearchDestroy(s);
  if(pp) thfPronunDestroy(pp);
  panic(cons,ewhere,ewhat,0);
  if(ses) thfSessionDestroy(ses);
  return 1;
}
