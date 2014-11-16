#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <memory.h>
#define SENSETIVE 3
#define FRESTIME 1000
#define REPTIME 10000
#define PACKETLEN sizeof(struct input_event)
#define MAXDEVLISTLEN 10001
#define PATTERNSIZE 50
#define PATHSIZE 50

struct input_event  tmpevent;

pthread_t theadingreadfromxbox, threadcheckandsendevent;

int fpxbox, fpdevlist;// fpkeyboard, fpmouse;

char devlistbuffer[MAXDEVLISTLEN];

char xbox_devpath[PATHSIZE ] = "/dev/input/event";
char devlist_path[PATHSIZE ] = "/proc/bus/input/devices";

char devpattern[] = "Microsoft XBox 360 Super Mouse";
char eventpattern[] = "event";

char eventidstr[10] ;

int btn_state[5][600];

int next[PATTERNSIZE];

int kmp(char *A, char *B, int *next)
{
    int j = -1, ret = 0 , i;
    for ( i = 0; A[i]; i++)
    {
        while (j != -1 && A[i] != B[j + 1]) j = next[j];
        if (A[i] == B[j + 1]) j++;
        if (!B[j + 1])
        {
            return i + 1;
            j = next[j];
        }
    }
    return -1;
}

void prekmp(char *B, int *next)
{
    int i, j;
    next[0] = -1;
    j = -1;
    for ( i = 1; B[i]; i++)
    {
        while (j != -1 && B[i] != B[j + 1])
            j = next[j];
        if (B[i] == B[j + 1]) j++;
        next[i] = j;
    }
}

void GetEventIdStr(char *text , char *ret)
{
    int i;
    for (i = 0 ; text[i] != ' ' && text[i] != '\0' && text[i] != '\n'; i++)
    {
        ret[i] = text[i];
    }
    ret[i] = 0;
}

void AddEventIdStrtoPath(char *primdevpath, char *eventidstr)
{
    int len = strlen(primdevpath), i;
    for (i = 0 ; eventidstr[i]; i++)
    {
        primdevpath[i + len] = eventidstr[i];
    }
    primdevpath[i + len] = 0;
}

void CompleteXboxDevPath(char *devlistbuffer, char *xbox_devpath)
{
    int st, i;
    memset(eventidstr , 0 , sizeof(eventidstr));
    prekmp(devpattern, next);
    st = kmp(devlistbuffer, devpattern , next);
    prekmp(eventpattern, next);
    st += kmp(&devlistbuffer[st], eventpattern , next);
    GetEventIdStr(&devlistbuffer[st], eventidstr);
    AddEventIdStrtoPath(xbox_devpath, eventidstr);
    printf("%s\n", xbox_devpath);
}

int CheckEnd()
{
    return (btn_state[EV_KEY][BTN_MODE] == 1);
}

static inline int CheckReport(struct  input_event *curevent)
{
    return curevent->type == 0 && curevent->code == 0 && curevent->value == 0;
}

void SendReport(int fp)
{
    struct  input_event report_event;
    memset(&report_event, 0, sizeof(report_event));
    gettimeofday(&report_event.time, NULL);
    report_event.type = EV_SYN;
    report_event.code = SYN_REPORT ;
    report_event.value = 0;
    write(fp, &report_event, sizeof(report_event));
}
static inline int CheckEvent(struct input_event *curevent , int type , int code )
{
    return curevent->type == type && curevent->code == code ;
}
void *TheadingReadfromXboxFunc( void *arg)
{
    int end_fg = 0;
    //printf("yeh1!!!\n");
    while (1)
    {
        //printf("yeh4!!!\n");
        usleep(1);
        read(fpxbox, &tmpevent, sizeof(tmpevent));
        if (end_fg && CheckReport(&tmpevent)) return NULL;
        btn_state[tmpevent.type][tmpevent.code] = tmpevent.value;
        //if (CheckReport(&tmpevent))
        //{
        //if (SendReport(end_fg)) return NULL;
        // }
        end_fg = CheckEnd();
        //printf("fpxbox:%d type: %d code:%d value:%d\n", fpxbox, tmpevent.type, tmpevent.code, tmpevent.value);
    }
    return NULL;
}
void SendEvent(struct input_event *curevent, int type, int code, int value, int fp)
{
    memset(curevent, 0, sizeof(curevent));
    gettimeofday(&curevent->time, NULL);
    curevent->type = type;
    curevent->code = code;
    curevent->value = value;
    write(fp, curevent, PACKETLEN);
}
int CheckBtnState(int i , int j)
{
    return btn_state[i][j];
}
void *TheadingCheckandSendEventFunc( void *arg)
{
    struct input_event newevent;
    //printf("yeh2!!!\n");
    while (1)
    {
        //printf("yeh3!!!\n");
        //fflush(stdout);
        usleep(35000);

        if (CheckBtnState(EV_KEY, BTN_TL) )
        {
            //printf("yehtl!!! %d\n", tmpevent.value);
            //fflush(stdout);
            SendEvent(&newevent, EV_REL, REL_WHEEL, -1, fpxbox);
            SendReport(fpxbox);
        }
        else if (CheckBtnState(EV_KEY, BTN_TR))
        {
            //printf("yehtr!!! %d\n", tmpevent.value);
            //fflush(stdout);
            SendEvent(&newevent, EV_REL, REL_WHEEL, 1, fpxbox);
            SendReport(fpxbox);
        }
    }
    return NULL;
}

int main()
{
    int fst = 1;
    int sum = 0 ;
    memset(btn_state, 0, sizeof(btn_state));
    fpdevlist = open(devlist_path, O_RDWR);
    memset(devlistbuffer, 0, sizeof(devlistbuffer));
    read(fpdevlist, &devlistbuffer, sizeof(devlistbuffer) - 1);
    //printf("%s\n", devlistbuffer);
    //printf("read finished!!\n");
    CompleteXboxDevPath(devlistbuffer, xbox_devpath);
    fpxbox = open(xbox_devpath, O_RDWR);

    pthread_create(&threadcheckandsendevent, NULL, TheadingCheckandSendEventFunc, "Processing Read from Xbox...");
    /*if (){
        pthread_cancel(pthx);
    }*/
    //printf("%d  %d\n", sum, PACKETLEN);
    pthread_create(&theadingreadfromxbox, NULL, TheadingReadfromXboxFunc, "Processing Read from Xbox...");

    pthread_join(theadingreadfromxbox, NULL);
    pthread_cancel(threadcheckandsendevent);
    pthread_join(threadcheckandsendevent, NULL);

    close(fpxbox);
    close(fpdevlist);
    return 0;
}
