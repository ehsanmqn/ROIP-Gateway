#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linphone/core.h>
#include <linphone/lpconfig.h>
#include <signal.h>
#include <curl/curl.h>
#include <pthread.h>
#include <sys/time.h>

//#define DEBUG_LOGS
#define AUDIO 0
#define VIDEO 1

#define INT_TO_VOIDPTR(i) ((void*)(intptr_t)(i))
#define VOIDPTR_TO_INT(p) ((int)(intptr_t)(p))

static bool_t running=TRUE;

/*
 * Drop AVA PBX call by extension
 */
static void DropCallByExtension(const char *extension)
{
    CURL *curl;
    CURLcode res;
    char url[] = "http://46.224.6.228:8000/ws/?action=DropCurrentCall&exten=";

    strcat(url, extension);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
    }
    else
    {
        return 0;
    }

    curl_easy_cleanup(curl);

    return 1;
}


/*
 * Stop function
 */
static void stop(int signum){
    running=FALSE;
}


/*
 * Registration state notification callback
 */
static void registration_state_changed(struct _LinphoneCore *lc,
                                       LinphoneProxyConfig *cfg,
                                       LinphoneRegistrationState cstate,
                                       const char *message)
{
    printf("New registration state %s for user id [%s] at proxy [%s]\n"
           ,linphone_registration_state_to_string(cstate)
           ,linphone_proxy_config_get_identity(cfg)
           ,linphone_proxy_config_get_addr(cfg));
}

/*
 * Call state notification callback
 */
int outgoingProgress = 0;
int streamRunning = 0;
struct timeval _time;
double seconds = 0, outgoingProgressTime = 0, streamRunningTime = 0, diff = 0;
static void call_state_changed(LinphoneCore *lc,
                               LinphoneCall *call,
                               LinphoneCallState cstate,
                               const char *msg)
{
    gettimeofday(&_time, NULL);
    seconds = (double)(_time.tv_usec) / 1000000 + (double)(_time.tv_sec);
    switch(cstate){
    case LinphoneCallOutgoingInit:
        printf("\n********* It is now outgoing init! time: %f\n", seconds);
        break;
    case LinphoneCallOutgoingProgress:
        if(!outgoingProgress) {
            outgoingProgressTime = seconds;
            printf("\n********* It is now outgoing progress! time: %f\n", seconds);
            outgoingProgress = 1;
        }
        break;
    case LinphoneCallOutgoingRinging:
        printf("\n********* It is now ringing remotely! time: %f\n", seconds);
        break;
    case LinphoneCallOutgoingEarlyMedia:
        printf("\n********* Receiving some early media. time: %f\n", seconds);
        break;
    case LinphoneCallConnected:
        printf("\n********* We are connected! time: %f\n", seconds);
        break;
    case LinphoneCallStreamsRunning:
        if(!streamRunning) {
            streamRunningTime = seconds;
            diff = streamRunningTime - outgoingProgressTime;
            printf("\n********* Media streams established! time: %f\n", streamRunningTime);
            printf("********* It is now outgoing progress! time: %f\n", outgoingProgressTime);
            printf("********* Media streams established! DIFF: %f\n\n", diff);
            streamRunning = 1;
        }
        break;
    case LinphoneCallEnd:
        printf("\n********* Call is terminated. time: %f\n", seconds);
        break;
    case LinphoneCallError:
        printf("\n********* Call failure ! time: %f\n", seconds);
        break;
    default:
        printf("\n********* Unhandled notification %i\n",cstate);
    }
}

/*
 * Codec enable function
 */
static void EnableCodecByIndex(int type,
                               LinphoneCore *lc,
                               int sel_index)
{
    PayloadType *pt;
    int index=0;
    const bctbx_list_t *node=NULL;

    if (type == AUDIO) {
        node=linphone_core_get_audio_codecs(lc);
    } else if(type==VIDEO) {
        node=linphone_core_get_video_codecs(lc);
    }

    for(;node!=NULL;node=bctbx_list_next(node)){
        if (index == sel_index || sel_index == -1) {
            pt=(PayloadType*)(node->data);
            linphone_core_enable_payload_type (lc,pt,TRUE);
            printf("%2d: %s (%d) %s\n", index, pt->mime_type, pt->clock_rate, "enabled");
        }
        index++;
    }
}


/*
 * Codec disable function
 */
static void DisableCodecByIndex(int type,
                                LinphoneCore *lc,
                                int sel_index)
{
    PayloadType *pt;
    int index=0;
    const bctbx_list_t *node=NULL;

    if (type == AUDIO) {
        node=linphone_core_get_audio_codecs(lc);
    } else if(type==VIDEO) {
        node=linphone_core_get_video_codecs(lc);
    }

    for(;node!=NULL;node=bctbx_list_next(node)){
        if (index == sel_index || sel_index == -1) {
            pt=(PayloadType*)(node->data);
            linphone_core_enable_payload_type (lc,pt,FALSE);
            printf("%2d: %s (%d) %s\n", index, pt->mime_type, pt->clock_rate, "disabled");
        }
        index++;
    }
}


/*
 * Codec list function
 */
static void ListCodecs(int type,
                       LinphoneCore *lc)
{
    PayloadType *pt;
    int index=0;
    const bctbx_list_t *node=NULL;

    if (type == AUDIO) {
        node=linphone_core_get_audio_codecs(lc);
    } else if(type==VIDEO) {
        node=linphone_core_get_video_codecs(lc);
    }

    for(;node!=NULL;node=bctbx_list_next(node)){
        pt=(PayloadType*)(node->data);
        printf("%2d: %s (%d) %s\n", index, pt->mime_type, pt->clock_rate,
               linphone_core_payload_type_enabled(lc,pt) ? "enabled" : "disabled");
        index++;
    }
}


/*
 * Register function
 */
int Register(LinphoneCore *lc,
             char* identity,
             char* password,
             LinphoneProxyConfig* proxy_cfg)
{
    LinphoneAddress *from;
    LinphoneAuthInfo *info;

    const char* server_addr;

    signal(SIGINT, stop);

#ifdef DEBUG_LOGS
    linphone_core_enable_logs(NULL); /*enable liblinphone logs.*/
#endif

    /*parse identity*/
    from = linphone_address_new(identity);
    if (from==NULL){
        printf("%s not a valid sip uri, must be like sip:toto@sip.linphone.org \n",identity);
        return 0;
    }

    if (password!=NULL){
        info=linphone_auth_info_new(linphone_address_get_username(from),NULL,password,NULL,NULL,NULL); /*create authentication structure from identity*/
        linphone_core_add_auth_info(lc,info); /*add authentication info to LinphoneCore*/
    }

    // configure proxy entries
    linphone_proxy_config_set_identity_address(proxy_cfg, from);            /*set identity with user name and domain*/
    server_addr = linphone_address_get_domain(from);                    /*extract domain address from identity*/
    linphone_proxy_config_set_server_addr(proxy_cfg, server_addr);      /* we assume domain = proxy server address*/
    linphone_proxy_config_enable_register(proxy_cfg, TRUE);             /*activate registration for this proxy config*/
    linphone_address_unref(from);                                       /*release resource*/

    linphone_core_add_proxy_config(lc,proxy_cfg);   /*add proxy config to linphone core*/
    linphone_core_set_default_proxy_config(lc,proxy_cfg);  /*set to default proxy*/

    /* main loop for receiving notifications and doing background linphonecore work: */
    while(linphone_proxy_config_get_state(proxy_cfg) !=  LinphoneRegistrationOk){
        linphone_core_iterate(lc); /* first iterate initiates registration */
        ms_usleep(500000);
    }
    return 1;
}


/*
 * Unregister function
 */
int Unregister(LinphoneCore *lc,
               LinphoneProxyConfig* proxy_cfg)
{
    /* Unregister */
    proxy_cfg = linphone_core_get_default_proxy_config(lc);     /* get default proxy config*/
    linphone_proxy_config_edit(proxy_cfg);                      /*start editing proxy configuration*/
    linphone_proxy_config_enable_register(proxy_cfg,FALSE);     /*de-activate registration for this proxy config*/
    linphone_proxy_config_done(proxy_cfg);                      /*initiate REGISTER with expire = 0*/

    while(linphone_proxy_config_get_state(proxy_cfg) !=  LinphoneRegistrationCleared){
        linphone_core_iterate(lc); /*to make sure we receive call backs before shutting down*/
        ms_usleep(50000);
    }

    return 1;
}


/*
 * Display stream status function
 */
static void RecordVoice()
{
    system("arecord --format=cd file.wav");
}

/*
 * Display stream status function
 */
static void VoiceActivityDetection(LinphoneCore *lc)
{
    LinphoneCall *call;
    LinphoneCallStats * audioStats;
    const bctbx_list_t *elem;
    char *tmp;
    float downloadedBandwidth, avgBandwidth = 0;

    elem=linphone_core_get_calls(lc);
    if (elem==NULL){
        printf("(empty)\n");
        return;
    }

    while(1)
    {
        call = (LinphoneCall*)elem->data;

        if(call)
        {
            audioStats = linphone_call_get_audio_stats(call);
            downloadedBandwidth = linphone_call_stats_get_download_bandwidth(audioStats);

            if(downloadedBandwidth > avgBandwidth + 4)
                printf("\n\n------------------ Incomming voice detected -----------------------\n\n");

            if(downloadedBandwidth < avgBandwidth - 5)
                printf("\n\n------------------ Silence detected -----------------------\n\n");

            avgBandwidth = (avgBandwidth + downloadedBandwidth) / 2;
        }
        ms_usleep(10);
    }
}

/*
 * Display stream status function
 */
static void DisplayStreamStatus(LinphoneCore *lc)
{
    LinphoneCall *call;

    const bctbx_list_t *elem;
    char *tmp;

    printf("Call states\n"
           "Down\t| Up\t| Late\t| RTP-S\t| A-Dir\t| Audio state \n"
           "------------------------------------------------------------------------\n");
    elem=linphone_core_get_calls(lc);
    if (elem==NULL){
        printf("(empty)\n");
    }else{
        for(;elem!=NULL;elem=elem->next){
            const char *flag;
            call=(LinphoneCall*)elem->data;
            LinphoneCallParams * callParams = linphone_call_get_params(call);
            LinphonePayloadType * payloadType = linphone_call_params_get_used_audio_payload_type(callParams);
            LinphoneCallStats * audioStats = linphone_call_get_audio_stats(call);
            float downloadedBandwidth = linphone_call_stats_get_download_bandwidth(audioStats);
            float uploadedBandwidth = linphone_call_stats_get_upload_bandwidth(audioStats);
            float cumulativeLatePacket = linphone_call_stats_get_late_packets_cumulative_number(audioStats);

            printf("%.2f\t| %.2f\t| %.2f\t| %d\t| %d\t| %s \n",
                   downloadedBandwidth,
                   uploadedBandwidth,
                   cumulativeLatePacket,
                   linphone_call_stats_get_rtp_stats(audioStats)->packet_sent,
                   linphone_call_params_get_audio_direction(callParams),
                   linphone_call_state_to_string(linphone_call_get_state(call))+strlen("LinphoneCall"));
        }
    }
}

/*
 * Display call states function
 */
static void DisplayCallStates(LinphoneCore *lc)
{
    LinphoneCall *call;
    const bctbx_list_t *elem;
    char *tmp;

    printf("Call states\n"
           "Id |            Destination              |      State      |    Flags   |\n"
           "------------------------------------------------------------------------\n");
    elem=linphone_core_get_calls(lc);
    if (elem==NULL){
        printf("(empty)\n");
    }else{
        for(;elem!=NULL;elem=elem->next){
            const char *flag;
            bool_t in_conference;
            call=(LinphoneCall*)elem->data;
            in_conference=(linphone_call_get_conference(call) != NULL);
            LinphoneCallStats * audioStats = linphone_call_get_audio_stats(call);
            float downloadedBandwidth = linphone_call_stats_get_download_bandwidth(audioStats);

            tmp=linphone_call_get_remote_address_as_string (call);
            flag=in_conference ? "conferencing" : "";
            flag=linphone_call_has_transfer_pending(call) ? "transfer pending" : flag;
            printf("%-2i | %-35s | %-15s | %s | %f\n",
                   VOIDPTR_TO_INT(linphone_call_get_user_data(call)),
                   tmp,
                   linphone_call_state_to_string(linphone_call_get_state(call))+strlen("LinphoneCall"),
                   flag);

            ms_free(tmp);
        }
    }
}


/*
 * Normal Call function
 */
int TestCall(LinphoneCore *lc, char *dest)
{
    pthread_t thread;
    LinphoneCall *call=NULL;
    LinphoneCallParams *cp = linphone_core_create_call_params (lc, NULL);

    signal(SIGINT,stop);

#ifdef DEBUG_LOGS
    linphone_core_enable_logs(NULL); /*enable liblinphone logs.*/
#endif

    if ( linphone_core_in_call(lc) )
    {
        printf("Terminate or hold on the current call first.\n");
        return 0;
    }

    linphone_call_params_enable_video (cp, FALSE);
//    linphone_call_params_enable_early_media_sending(cp,TRUE);
//    linphone_call_params_set_audio_bandwidth_limit(cp, 22);
    linphone_call_params_enable_low_bandwidth(cp, 1);
    linphone_call_params_add_custom_sdp_media_attribute(cp, 0, "fmtp", "110 maxplaybackrate=8000; sprop-maxcapturerate=8000; maxaveragebitrate=10000");
//    linphone_call_params_set_audio_direction(cp, 2);
//    linphone_call_params_add_custom_header(cp, "Ehsan", "ehsan");


    linphone_core_iterate(lc);

    linphone_call_params_set_audio_direction(cp, 1);
//    int rc = pthread_create(&thread, NULL, RecordVoice, NULL);
    // Setting file to play on call
    linphone_core_set_use_files(lc, 1);
    linphone_core_set_play_file(lc, "vm.wav");

    ms_usleep(1000000);
    if ((call = linphone_core_invite_with_params(lc, dest, cp)) == NULL)
    {
        printf("****** Error from linphone_core_invite.\n");
    }
    else
    {
        printf("****** Call initiated to %s", dest);
    }


    //    linphone_call_params_unref(cp);
    /* main loop for receiving notifications and doing background linphonecore work: */

    LinphoneCallState callState;

    while (running && call)
    {

        linphone_core_iterate(lc);
//        DisplayCallStates(lc);
//        DisplayStreamStatus(lc);

        callState = linphone_call_get_state(call);
        call_state_changed(lc, call, callState, "");

        if(callState != LinphoneCallEnd)
//            continue;
            ms_usleep(500);
        else
            break;
    }

    if (call && linphone_call_get_state(call) != LinphoneCallEnd)
    {
        /* terminate the call */
        printf("Terminating the call...\n");
        linphone_core_terminate_call(lc,call);

        /*at this stage we don't need the call object */
        linphone_call_unref(call);
    }

    return 0;
}


/*
 * Customized for ROIP Gateway Call function
 */
int RoipCall(LinphoneCore *lc, char *dest)
{
    pthread_t thread;
    LinphoneCall *call=NULL;
    LinphoneCallParams *cp = linphone_core_create_call_params (lc, NULL);

    signal(SIGINT,stop);

#ifdef DEBUG_LOGS
    linphone_core_enable_logs(NULL); /*enable liblinphone logs.*/
#endif

    // First Drop Call in PBX
    DropCallByExtension(dest);

    if ( linphone_core_in_call(lc) )
    {
        printf("Terminate or hold on the current call first.\n");
        return 0;
    }

    linphone_call_params_enable_video (cp, FALSE);
    linphone_call_params_enable_early_media_sending(cp,TRUE);
    linphone_call_params_enable_low_bandwidth(cp, 1);

    linphone_core_iterate(lc);
    if ((call = linphone_core_invite(lc, dest)) == NULL)
    {
        printf("Error from linphone_core_invite.\n");
    }
    else
    {
        printf("Call initiated to %s", dest);
    }

    linphone_call_params_unref(cp);

    int rc = pthread_create(&thread, NULL, VoiceActivityDetection, (LinphoneCore *)lc);

    for(int i=1; i < 50; i++){
        linphone_core_iterate(lc);
        ms_usleep(500000);
    }

    return 1;
}


/*
 * Main function
 */
LinphoneCore *lc;
int main(int argc, char *argv[])
{
    LinphoneProxyConfig* proxy_cfg;
    LinphoneCoreVTable vtable={0};

    const char **dev;
    char* identity=NULL;
    char* password=NULL;

    identity = "sip:09034542613@94.182.177.99:5070";
    password = "CJ9tDPDsbZ";

    /*
     Fill the LinphoneCoreVTable with application callbacks.
     All are optional. Here we only use the registration_state_changed callbacks
     in order to get notifications about the progress of the registration.
    */
    vtable.registration_state_changed = registration_state_changed;

    /*
     Instanciate a LinphoneCore object given the LinphoneCoreVTable
    */
    lc = linphone_core_new(&vtable, NULL, NULL, NULL);

    /* create proxy config */
    proxy_cfg = linphone_core_create_proxy_config(lc);

    /* parse identity */
//    Register(lc, identity, password, proxy_cfg);

    /* Configure codecs */
    printf("\n***** Codec configuration\n");
    DisableCodecByIndex(AUDIO, lc, 1);   // Disable speex 16000
    DisableCodecByIndex(AUDIO, lc, 3);   // Disable PCMU
    DisableCodecByIndex(AUDIO, lc, 4);   // Disable PCMA
    DisableCodecByIndex(AUDIO, lc, 0);   // Disable OPUS 48000

    printf("\n***** List codecs\n");
    ListCodecs(AUDIO, lc);              // List codecs

    /* Configure sound card */
    printf("\n***** Sound card configuration\n");

    dev = linphone_core_get_sound_devices(lc);
    for(int i = 0; dev[i] != NULL; ++i){
        printf("%i: %s\n", i, dev[i]);
    }

    linphone_core_set_ringer_device(lc, dev[0]);
    linphone_core_set_playback_device(lc, dev[0]);
    linphone_core_set_capture_device(lc, dev[0]);
    printf("Using sound device %s\n\n", dev[0]);


    /* Register UA */
    if(Register(lc, identity, password, proxy_cfg)) {
        printf("\n***** Registered successfully\n");
    }
    else {
        printf("\n***** Registeration failed\n");
    }

    /* Fill the LinphoneCoreVTable with application callbacks */
    vtable.call_state_changed = call_state_changed;


    /* Make call */
    TestCall(lc, "*1110112@94.182.177.99:5070");

    /* Unregister UA */
//    if(Unregister(lc, proxy_cfg))
//        printf("\n***** Unregistered successfully\n");

    return 0;
}
