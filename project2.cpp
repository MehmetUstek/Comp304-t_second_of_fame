#include <cstdlib>     /* srand, rand */
#include <ctime>       /* time */
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <string>
#include <semaphore.h>
#include <sys/time.h>
#include <queue>
#include <pthread.h>
#include <random>


int total_commentator_number;
int question_number;
float question_probability;
int time_speak;
float probability_breaking_news;
struct timeval start, current;
bool isTerminal_state = true;
int response_counter;
bool is_breaking_news = false;
pthread_cond_t* condvar_ptr;
// Function declarations
int pthread_sleep(double seconds);
void parse_command_line_arguments(int argc, char **argv, int *total_question_number, int *total_commentator_number, float *question_probability, int *time_speak, float *probability_of_breaking_news);
double random_double();
void *moderator_thread(void *id);
void *commentator_thread_main(void *id_ptr);
void *breaking_news_thread(void *arg);
int pthread_sleep (double seconds);
sem_t *commentator_mutex;
sem_t done_mutex;
sem_t question_mutex;
sem_t breaking_news_mutex;

FILE *commentator_log_file;
pthread_mutex_t sameLock = PTHREAD_MUTEX_INITIALIZER;
// Commentator struct
class Commentator {
public:
    unsigned int id;
    double t_speak;
    Commentator( unsigned int _id, double _t_speak) {
        id=_id;
        t_speak = _t_speak;
    }
};
enum CommentatorStates {
    Question_Asked, ANSWER, DONT_ANSWER, WAITING, BREAKING_NEWS
};
class CommentatorQueue {
    private:
    std::queue<Commentator> queue;
    pthread_mutex_t lock{};
    std::string queue_to_string(std::queue<Commentator> q) {
        std::string str_rep;
        while (!q.empty()) {
            str_rep += std::to_string(q.front().id) + " ";
            q.pop();
        }
        return str_rep;
    }
    public:
    void push(const Commentator &commentator) {
        pthread_mutex_lock(&lock);
        queue.push(commentator);
        pthread_mutex_unlock(&lock);
    }

    Commentator &front() {
        pthread_mutex_lock(&lock);
        Commentator &return_val = queue.front();
        pthread_mutex_unlock(&lock);
        return return_val;
    }

    Commentator &front_and_pop() {
        pthread_mutex_lock(&lock);
        Commentator &return_val = queue.front();
        queue.pop();
        pthread_mutex_unlock(&lock);
        return return_val;
    }

    void pop() {
        pthread_mutex_lock(&lock);
        queue.pop();
        pthread_mutex_unlock(&lock);
    }
    void popAll(){
        pthread_mutex_lock(&lock);
        while(!queue.empty()){
            front_and_pop();
        }
        pthread_mutex_unlock(&lock);
    }

    bool empty() {
        pthread_mutex_lock(&lock);
        bool return_val = queue.empty();
        pthread_mutex_unlock(&lock);
        return return_val;
    }

    int size() {
        pthread_mutex_lock(&lock);
        int return_val = queue.size();
        pthread_mutex_unlock(&lock);
        return return_val;
    }

    std::string to_string() {
        pthread_mutex_lock(&lock);
        std::string str_rep = queue_to_string(queue);
        pthread_mutex_unlock(&lock);
        return str_rep;
    }
    CommentatorQueue() {
        lock = sameLock;
    }
};

CommentatorQueue queue;

int main(int argc, char *argv[])
{
    printf("Please wait!\n");
    commentator_log_file = fopen("./commentator.log", "w+");
    parse_command_line_arguments(argc, argv, &question_number, &total_commentator_number, &question_probability, &time_speak, &probability_breaking_news);
    struct timeval tp{};
    gettimeofday(&tp, nullptr);
    printf("Based on your question number:%d and commentator number:%d\nTime to Speak:%d\nThis will approximately take:%d seconds\n",question_number,total_commentator_number,time_speak, question_number*total_commentator_number*time_speak/2);
    pthread_t moderatorThread, breaking_newsThread;
    pthread_t *commentatorThread = new pthread_t[total_commentator_number];
    commentator_mutex = new sem_t[total_commentator_number];
    sem_init(&question_mutex, 0, 0);
    sem_init(&done_mutex, 0, 0);
    sem_init(&breaking_news_mutex, 0, 0);
    for (int i = 0; i < total_commentator_number; ++i){
        sem_init(commentator_mutex + i, 0, 0);
    }

    pthread_create(&moderatorThread,NULL,&moderator_thread,NULL);

    for (int i = 0; i < total_commentator_number; ++i){
        pthread_create(commentatorThread + i, NULL, &commentator_thread_main, new int(i));
    }
    pthread_create(&breaking_newsThread, NULL, &breaking_news_thread, NULL);

    double random;
    while (isTerminal_state)
    {
        pthread_sleep(1.0);
        random = random_double();

        if (random < probability_breaking_news)
            sem_post(&breaking_news_mutex);
    }

    pthread_join(moderatorThread, NULL);

    for (int i = 0; i < total_commentator_number; ++i){
        pthread_join(commentatorThread[i], NULL);
    }

    pthread_cancel(breaking_newsThread);

    sem_destroy(&question_mutex);
    sem_destroy(&done_mutex);
    sem_destroy(&breaking_news_mutex);

    for (int i = 0; i < total_commentator_number; ++i){
        sem_destroy(commentator_mutex + i);
    }
    fprintf(commentator_log_file,"-----------------------\n");
    fprintf(commentator_log_file,"End of Discussion");
    printf("The Debate is over, please check commentator.log file\n");
    fclose(commentator_log_file);
}

void log_time()
{
    gettimeofday(&current, NULL);

    int diff = (current.tv_sec * 1000000 + current.tv_usec) -
    (start.tv_sec * 1000000 + start.tv_usec);
    diff /= 1000;
    int min = diff / 60000;
    float sec = diff % 60000;
    sec /= 1000.0;
    fprintf(commentator_log_file,"[");
    if (min < 10){
        fprintf(commentator_log_file,"0");
    }

    fprintf(commentator_log_file,"%d", min);
    fprintf(commentator_log_file,":");
    if (sec < 10){
        fprintf(commentator_log_file,"0");
    }
    fprintf(commentator_log_file,"%.3f] ", sec);
}

void *moderator_thread(void *arg)
{
    int id;
    gettimeofday(&start, NULL);

    for (int i = 0; i < question_number; ++i)
    {
        response_counter = 0;
        log_time();
        fprintf(commentator_log_file,"Moderator asks question %d\n", i + 1);
        sem_post(&done_mutex);
        for (int i = 0; i < total_commentator_number; ++i){
            sem_wait(&question_mutex);
        }
        while (!queue.empty())
        {
            Commentator comm = queue.front_and_pop();
            id = comm.id;
            sem_post(commentator_mutex + id);
            sem_wait(&question_mutex);
        }
    }
    isTerminal_state = false;
    pthread_exit(NULL);
}

double random_double() 
{
    return (double) rand() / RAND_MAX;
}
int pthread_sleep(double seconds){
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    if(pthread_mutex_init(&mutex,NULL)){
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL)){
        return -1;
    }
    condvar_ptr = &conditionvar;
    struct timeval tp;
    struct timespec timetoexpire;
    // When to expire is an absolute time, so get the current time and add
    // it to our delay time
    gettimeofday(&tp, NULL);
    long new_nsec = tp.tv_usec * 1000 + (seconds - (long)seconds) * 1e9;
    timetoexpire.tv_sec = tp.tv_sec + (long)seconds + (new_nsec / (long)1e9);
    timetoexpire.tv_nsec = new_nsec % (long)1e9;

    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);

    //Upon successful completion, a value of zero shall be returned
    return res;
}
void* breaking_news_thread(void* arg)
{
    while (true)
    {
        sem_wait(&breaking_news_mutex);
        is_breaking_news = false;
        pthread_cond_signal(condvar_ptr);
        log_time();
        fprintf(commentator_log_file,"Breaking news!\n");
        pthread_sleep(5.0);
        log_time();
        fprintf(commentator_log_file,"Breaking news ends\n");
        int temp;
        // sem_getvalue(&moderator_mutex,&temp);
        // printf("%d",temp);
        if(sem_trywait(&question_mutex)!=0){
            sem_post(&question_mutex);   
        } 
    }
    pthread_exit(NULL);
}
void parse_command_line_arguments(int argc, char **argv, int *total_question_number, int *total_commentator_number, float *question_probability, int *time_speak, float *probability_of_breaking_news) 
{
    int commentator_number = 5;
    int question_number = 5;
    float probability = 0.5;
    int t_speak;
    int queue_log_start = 0;
    float probability_breaking_news=0.05;

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp("-n", argv[i]) == 0) {
            int commentators = atoi(argv[i + 1]);
            if (commentators > 0) {
                commentator_number = commentators;
            } else {
                std::cout << "You entered an illegal value for simulation time, " << commentator_number
                          << " will be used as default" << std::endl;
            }
        } else if (strcmp("-p", argv[i]) == 0) {
            float prob_arg = atof(argv[i + 1]);
            // user entered 0 as probability
            if (strcmp(argv[i + 1], "0") == 0) {
                probability = 0;
            } else if (prob_arg <= 0.0 || prob_arg > 1.0) {
                std::cout << "You entered an illegal value for probability, " << probability
                          << " will be used as default" << std::endl;
            } else { 
                probability = prob_arg;
            }
        } else if (strcmp("-t", argv[i]) == 0) {
            int speak_time = atoi(argv[i + 1]);
            if (speak_time > 0) {
                t_speak = speak_time;
            } else {
                std::cout << "You entered an illegal value for speak time, " << t_speak
                          << " will be used as default" << std::endl;
            }
        }
        else if (strcmp("-q", argv[i]) == 0) {
            int questionNum = atoi(argv[i + 1]);
            if (questionNum > 0) {
                question_number = questionNum;
            } else {
                std::cout << "You entered an illegal value for question number, " << question_number
                          << " will be used as default" << std::endl;
            }
        }
        else if (strcmp("-b", argv[i]) == 0) {
            float prob_arg = atof(argv[i + 1]);
            // user entered 0 as probability
            if (strcmp(argv[i + 1], "0") == 0) {
                probability_breaking_news = 0;
            } else if (prob_arg <= 0.0 || prob_arg > 1.0) {
                std::cout << "You entered an illegal value for probability of breaking news, " << probability_breaking_news
                          << " will be used as default" << std::endl;
            } else { 
                probability_breaking_news = prob_arg;
            }
        }
    }
    *total_commentator_number = commentator_number;
    *total_question_number = question_number;
    *question_probability = probability;
    *time_speak = t_speak;
    *probability_of_breaking_news = probability_breaking_news;
}
void *commentator_thread_main(void *id_ptr)
{
    int id = *((int *)id_ptr);
    free(id_ptr);
    bool flag;
    for (int i = 0; i < question_number; ++i)
    {
        sem_wait(&done_mutex);
        flag = true;
        double random = random_double();
        float t_speak;
        if (random < question_probability)
        {
            log_time();
            fprintf(commentator_log_file,"Commentator #%d generates answer, position in queue %d\n", id, queue.size());
            double rand = random_double();
            std::random_device rd;
            std::mt19937 e2(rd());
            std::uniform_real_distribution<> dist(1, time_speak);
            t_speak=dist(e2);
            Commentator comm = Commentator(id,t_speak);
            queue.push(comm);
            flag = false;
        }
        if (++response_counter < total_commentator_number)
            sem_post(&done_mutex);
        sem_post(&question_mutex);
        if (flag)
        {
            pthread_sleep(0.001);
            continue;
        }
        sem_wait(commentator_mutex + id);
        log_time();
        fprintf(commentator_log_file,"Commentator #%d's turn to speak for %.3f seconds\n", id, t_speak);
        is_breaking_news = true;
        pthread_sleep(t_speak);
        if (is_breaking_news) 
        {
            log_time();
            fprintf(commentator_log_file,"Commentator #%d finished speaking\n", id);
            sem_post(&question_mutex);
        }
        else if (!is_breaking_news)
        {
            log_time();
            fprintf(commentator_log_file,"Commentator #%d is cut short due to a breaking news\n", id);
        }
    }
    pthread_exit(NULL);
}