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
#define MAXMACROSTRLEN 100


struct input_event  tmpevent;

pthread_t theadingreadfromxbox, threadingwheel , theadingreadcmd;

int fpxbox, fpdevlist;// fpkeyboard, fpmouse;

char devlistbuffer[MAXDEVLISTLEN];

char xbox_devpath[PATHSIZE ] = "/dev/input/event";
char devlist_path[PATHSIZE ] = "/proc/bus/input/devices";

char devpattern[] = "Microsoft XBox 360 Super Mouse";
char eventpattern[] = "event";

char eventidstr[10] ;

int btn_state[5][600];

int keyid_table[270];

char btnback_macrostr[MAXMACROSTRLEN];
char btnstart_macrostr[MAXMACROSTRLEN];

int next[PATTERNSIZE];

int letter_keyid_table[] = {KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
                            KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
                           };



static inline void setkey(char a , int b)
{
    keyid_table[(a)] = (b);
}
void InitKeyidTable()
{
    int i;
    for (i = 0 ; i < 10; i++)
    {
        if (!i)
        {
            keyid_table['0'] = KEY_0;
        }
        else
        {
            keyid_table['0' + i] = i + 1;
        }
    }
    for (i = 0 ; i < 26; i++)
    {
        keyid_table['a' + i ] = letter_keyid_table[i];
    }
    setkey('=', KEY_EQUAL);
    setkey('-', KEY_MINUS);
    setkey(' ', KEY_BACKSPACE);
    setkey('[', KEY_LEFTBRACE);
    setkey(']', KEY_RIGHTBRACE);
    setkey(';', KEY_SEMICOLON);
    setkey('\'', KEY_APOSTROPHE);
    setkey(',', KEY_COMMA);
    setkey('.', KEY_DOT);
    setkey('\\', KEY_SLASH);
}
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

int CheckBtnState(int i , int j)
{
    return btn_state[i][j];
}
static inline int CheckEvent(struct input_event *curevent , int type , int code )
{
    return curevent->type == type && curevent->code == code ;
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


void SendKeyfromChar(char x, int fp)
{
    struct input_event curevent;
    int keyid = keyid_table[x];
    //printf("keyid : %d\n", keyid);
    SendEvent(&curevent, EV_KEY, keyid, 1, fp);
    SendReport(fp);
    SendEvent(&curevent, EV_KEY, keyid, 0, fp);
    SendReport(fp);
}

void *TheadingReadandProsKeyfromXboxFunc( void *arg)
{
    int end_fg = 0, i;
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
        if (CheckBtnState(EV_KEY, BTN_SELECT))
        {
            puts("btn_back");
            puts(btnback_macrostr);
            for (i = 0; btnback_macrostr[i]; i++) SendKeyfromChar(btnback_macrostr[i], fpxbox);
            btn_state[EV_KEY][BTN_SELECT] = 0;
        }
        else if (CheckBtnState(EV_KEY, BTN_START))
        {
            puts("btn_start");
            puts(btnstart_macrostr);
            for (i = 0; btnstart_macrostr[i]; i++) SendKeyfromChar(btnstart_macrostr[i], fpxbox);
            btn_state[EV_KEY][BTN_START] = 0;
        }
        end_fg = CheckEnd();
        printf("fpxbox:%d type: %d code:%d value:%d\n", fpxbox, tmpevent.type, tmpevent.code, tmpevent.value);
    }
    return NULL;
}



void *TheadingReadCmdFunc(void *arg)
{
    char cmd[50], str_a[50], str_b[MAXMACROSTRLEN];
    int fail_fg = 0;
    puts("command format:");
    printf("set btnback [string]\t\t type [string] when btnback pressed\n");
    printf("set btnstart [string]\t\t type [string] when btnstart pressed\n");
    while (scanf(" %s", cmd) == 1)
    {
        fail_fg = 1;
        //puts(str_b);
        if (strcmp(cmd, "set") == 0)
        {
            scanf(" %s", str_a) ;
            if (strcmp(str_a, "btnback") == 0)
            {
                if (scanf(" %s", str_b) == 1)
                {
                    fail_fg = 0;
                    strcpy(btnback_macrostr, str_b);

                }
                //puts(btnback_macrostr);
            }
            else if (strcmp(str_a, "btnstart") == 0)
            {
                if (scanf(" %s", str_b) == 1)
                {
                    fail_fg = 0;
                    strcpy(btnstart_macrostr, str_b);
                }
                //puts(btnstart_macrostr);
            }
        }
        if (fail_fg) puts("invalid command!");
    }
}


void *TheadingWheelFunc( void *arg)
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
    InitKeyidTable();
    memset(btn_state, 0, sizeof(btn_state));
    fpdevlist = open(devlist_path, O_RDWR);
    memset(devlistbuffer, 0, sizeof(devlistbuffer));
    read(fpdevlist, &devlistbuffer, sizeof(devlistbuffer) - 1);
    //printf("%s\n", devlistbuffer);
    //printf("read finished!!\n");
    CompleteXboxDevPath(devlistbuffer, xbox_devpath);
    fpxbox = open(xbox_devpath, O_RDWR);

    pthread_create(&threadingwheel, NULL, TheadingWheelFunc, "Processing Wheel ");

    pthread_create(&theadingreadcmd, NULL, TheadingReadCmdFunc, "Processing Read from Console...");

    /*if (){
        pthread_cancel(pthx);
    }*/
    //printf("%d  %d\n", sum, PACKETLEN);
    pthread_create(&theadingreadfromxbox, NULL, TheadingReadandProsKeyfromXboxFunc, "Processing Read from Xbox...");

    //    END
    pthread_join(theadingreadfromxbox, NULL);

    pthread_cancel(threadingwheel);
    pthread_join(threadingwheel, NULL);

    pthread_cancel(theadingreadcmd);
    pthread_join(theadingreadcmd, NULL);

    close(fpxbox);
    close(fpdevlist);
    return 0;
}
