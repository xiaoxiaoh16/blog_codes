#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace std;

#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/queue.h>

#define MAX_TIMER_NUM		1000	/**< max timer number	*/
#define TIMER_START 		1	/**< timer start(second)*/
#define TIMER_TICK 		1	/**< timer tick(second)	*/
#define INVALID_TIMER_ID 	(-1)	/**< invalid timer ID	*/
#define SECONDS 1000000000

typedef int timer_id;

/**
 * The type of callback function to be called by timer scheduler when a timer
 * has expired.
 *
 * @param id		The timer id.
 * @param user_data     The user data.
 * $param len		The length of user data.
 */
typedef int timer_expiry(timer_id id, void *user_data, int len);

/**
 * The type of the timer
 */
struct timer {
    LIST_ENTRY(timer) entries;	/**< list entry		*/

    timer_id id;			/**< timer id		*/

    int interval;			/**< timer interval(second)*/
    int elapse; 			/**< 0 -> interval 	*/

    timer_expiry *cb;		/**< call if expiry 	*/
    void *user_data;		/**< callback arg	*/
    int len;			/**< user_data length	*/
};

/**
 * The timer list
 */
struct timer_list {
    LIST_HEAD(listheader, timer) header;	/**< list header 	*/
    int num;				/**< timer entry number */
    int max_num;				/**< max entry number	*/

    void (*old_sigfunc)(int);		/**< save previous signal handler */
    void (*new_sigfunc)(int);		/**< our signal handler	*/

    struct itimerval ovalue;		/**< old timer value */
    struct itimerval value;			/**< our internal timer value */
};

int init_timer(int count);

int destroy_timer(void);

timer_id
add_timer(int interval, timer_expiry *cb, void *user_data, int len);

int del_timer(timer_id id);

static struct timer_list timer_list;

static void sig_func(int signo);

/**
 * Create a timer list.
 *
 * @param count    The maximum number of timer entries to be supported 
 *			initially.  
 *
 * @return         0 means ok, the other means fail.
 */
int init_timer(int count)
{
    int ret = 0;

    if(count <=0 || count > MAX_TIMER_NUM) {
        printf("the timer max number MUST less than %d.\n", MAX_TIMER_NUM);
        return -1;
    }

    memset(&timer_list, 0, sizeof(struct timer_list));
    LIST_INIT(&timer_list.header);
    timer_list.max_num = count;

    /* Register our internal signal handler and store old signal handler */
    if ((timer_list.old_sigfunc = signal(SIGALRM, sig_func)) == SIG_ERR) {
        return -1;
    }
    timer_list.new_sigfunc = sig_func;

    /* Setting our interval timer for driver our mutil-timer and store old timer value */
    timer_list.value.it_value.tv_sec = TIMER_START;
    timer_list.value.it_value.tv_usec = 0;
    timer_list.value.it_interval.tv_sec = TIMER_TICK;
    timer_list.value.it_interval.tv_usec = 0;
    ret = setitimer(ITIMER_REAL, &timer_list.value, &timer_list.ovalue);

    return ret;
}

/**
 * Destroy the timer list.
 *
 * @return          0 means ok, the other means fail.
 */
int destroy_timer(void)
{
    struct timer *node = NULL;

    if ((signal(SIGALRM, timer_list.old_sigfunc)) == SIG_ERR) {
        return -1;
    }

    if((setitimer(ITIMER_REAL, &timer_list.ovalue, &timer_list.value)) < 0) {
        return -1;
    }

    while (!LIST_EMPTY(&timer_list.header)) {/* Delete. */
        node = LIST_FIRST(&timer_list.header);
        LIST_REMOVE(node, entries);
        /* Free node */
        printf("Remove id %d\n", node->id);
        if (node->user_data)
            free(node->user_data);
        free(node);
    }

    memset(&timer_list, 0, sizeof(struct timer_list));

    return 0;
}

/**
 * Add a timer to timer list.
 *
 * @param interval  The timer interval(second).  
 * @param cb  	    When cb!= NULL and timer expiry, call it.  
 * @param user_data Callback's param.  
 * @param len  	    The length of the user_data.  
 *
 * @return          The timer ID, if == INVALID_TIMER_ID, add timer fail.
 */
timer_id add_timer(int interval, timer_expiry *cb, void *user_data, int len)
{
    struct timer *node = NULL;

    if (cb == NULL || interval <= 0) {
        return INVALID_TIMER_ID;
    }

    if(timer_list.num < timer_list.max_num) {
        timer_list.num++;
    } else {
        return INVALID_TIMER_ID;
    }

    if((node = (timer*)malloc(sizeof(struct timer))) == NULL) {
        return INVALID_TIMER_ID;
    }
    if(user_data != NULL || len != 0) {
        node->user_data = malloc(len);
        memcpy(node->user_data, user_data, len);
        node->len = len;
    } else {
        node->user_data = NULL;
    }

    node->cb = cb;
    node->interval = interval;
    node->elapse = 0;
    node->id = timer_list.num;

    LIST_INSERT_HEAD(&timer_list.header, node, entries);

    return node->id;
}

/* Tick Bookkeeping */
static void sig_func(int signo)
{
    struct timer *node = timer_list.header.lh_first;
    for ( ; node != NULL; node = node->entries.le_next) {
        node->elapse++;
        if(node->elapse >= node->interval) {
            node->elapse = 0;
            node->cb(node->id, node->user_data, node->len);
        }
    }
}

static char *fmt_time(char *tstr)
{
    time_t t;

    t = time(NULL);
    strcpy(tstr, ctime(&t));
    tstr[strlen(tstr)-1] = '\0';

    return tstr;
}

/* Unit Test */
timer_id id[5];
int alarmCnt = 0;
int timer_cb(timer_id id, void *arg, int len)
{
    char tstr[200];

    alarmCnt ++;
    /* XXX: Don't use standard IO in the signal handler context, I just use it demo the timer */
    printf("hello [%s]/id %d: timer '%s' cb is here.\n", fmt_time(tstr), id, (char*)arg);
    return 0;
}

int main(void)
{
    char arg[50];
    char tstr[200];
    int ret;

    // set timer
    struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec= 0.1 * SECONDS;

    init_timer(MAX_TIMER_NUM);

    id[0] = add_timer(2, timer_cb, (void*)"a", sizeof("a"));
    id[1] = add_timer(3, timer_cb, (void*)"b", sizeof("b"));
    id[2] = add_timer(5, timer_cb, (void*)"c", sizeof("c"));
    id[3] = add_timer(7, timer_cb, (void*)"d", sizeof("d"));
    id[4] = add_timer(9, timer_cb, (void*)"e", sizeof("e"));

    for (int cnt = 0; alarmCnt < 45; cnt++) {
        nanosleep(&time, NULL);
    }

    ret = destroy_timer();
    printf("main: %s destroy_timer, ret=%d\n", fmt_time(tstr), ret);
    return 0;
}
