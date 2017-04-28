#include <iostream>
#include <sstream>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <cmath>

#define COLOR_PAIR_BLACK_YELLOW 1
#define COLOR_PAIR_BLACK_GREEN 2
#define COLOR_PAIR_BLACK_RED 3


struct RequestInfo {
    double t;
    std::string site_id;

    RequestInfo(double _t, std::string _site_id) : t(_t), site_id(_site_id) {};
};

struct Avg {
    std::vector<double> times;
    double total_time;
    unsigned count;

    Avg(double _t) : times({_t}), total_time(_t), count(1) {};
    void add_t(double t) {
        times.push_back(t);
        total_time += t;
        count++;
    };
};

struct Stat {
    std::map<std::string, Avg> averages;

    void consume(std::string site_id, double t) {
        auto it = averages.find(site_id);
        if (it == averages.end()) {
            averages.emplace(site_id, Avg(t));
        } else {
            it->second.add_t(t);
        }
    };
};

struct APP {
    bool is_screen_dirty;
    bool is_window_dirty;
    bool quit;
    std::vector<RequestInfo> messages;
    std::mutex messages_mutex;
    Stat stat;

    APP() : is_screen_dirty(true),
            is_window_dirty(true),
            quit(false) {};
} app;

void on_window_resize(int);
void refresh_screen();
void listen_input(APP&);
void check_screen_state();

int main() {
    initscr();
    signal(SIGWINCH, on_window_resize);

    nodelay(stdscr, true);
    noecho();
    curs_set(0);
    start_color();

    init_pair(COLOR_PAIR_BLACK_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_PAIR_BLACK_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PAIR_BLACK_RED, COLOR_RED, COLOR_BLACK);

    std::thread listen_thread(listen_input, std::ref(app));

    int ch;
    for (;;) {
        struct timespec t_sleep;
        t_sleep.tv_sec = 0;
        t_sleep.tv_nsec = 10000000l;
        nanosleep(&t_sleep, nullptr);

        if ((ch = getch()) != ERR) {
            if (ch == 27) app.quit = true;
        }

        check_screen_state();

        if (app.quit) {
            listen_thread.detach();
            break;
        }
    }

    if (listen_thread.joinable()) listen_thread.join();

    endwin();

    return EXIT_SUCCESS;
}

void on_window_resize(int sig) {
    app.is_window_dirty = true;

    refresh_screen();
}

void check_screen_state() {
    if (app.is_screen_dirty || app.is_window_dirty) refresh_screen();
}

void refresh_screen() {
    if (app.is_window_dirty) {
        endwin();
        refresh();

        app.is_window_dirty = false;
    }

    clear();

    unsigned colw_t, colw_site_id, colw_avg, colw_chg;
    colw_t = (unsigned) floor(stdscr->_maxx * 0.15);
    colw_avg = colw_t;
    colw_chg = colw_t;
    colw_site_id = stdscr->_maxx - (3 * colw_t) - 2;

    int line = 1;
    for (auto it = app.messages.rbegin(); it != app.messages.rend(); it++) {
        wmove(stdscr, line, 1);

        // Print elapsed time (t).
        char t_str[colw_t + 1];
        bzero(t_str, colw_t + 1);
        sprintf(t_str, "%*.2f ", colw_t - 1, it->t);
        attron(A_BOLD);
        waddstr(stdscr, t_str);
        attroff(A_BOLD);

        // Print site id.
        wmove(stdscr, line, colw_t + 1);
        char site_id_str[colw_site_id + 1];
        bzero(site_id_str, colw_site_id + 1);
        strncpy(site_id_str, it->site_id.c_str(), colw_site_id - 1);
        waddstr(stdscr, site_id_str);

        auto stat_it = app.stat.averages.find(it->site_id);

        // Print average.
        double avg = stat_it->second.total_time / stat_it->second.count;
        wmove(stdscr, line, colw_t + colw_site_id + 1);
        char avg_str[colw_avg + 1];
        bzero(avg_str, colw_avg + 1);
        sprintf(avg_str, "%*.2f ", -colw_avg + 1, avg);
        attron(COLOR_PAIR(COLOR_PAIR_BLACK_YELLOW));
        waddstr(stdscr, avg_str);
        attroff(COLOR_PAIR(COLOR_PAIR_BLACK_YELLOW));

        // Print change.
        double change = ((it->t - avg) / avg) * 100.0;
        wmove(stdscr, line, colw_t + colw_site_id + colw_avg + 1);
        char chg_str[colw_chg + 1];
        bzero(chg_str, colw_chg + 1);
        sprintf(chg_str, "%*.2f %%", colw_chg - 2, change);
        auto color_pair = change >= 0 ? COLOR_PAIR_BLACK_GREEN : COLOR_PAIR_BLACK_RED;
        attron(COLOR_PAIR(color_pair));
        waddstr(stdscr, chg_str);
        attroff(COLOR_PAIR(color_pair));

        line++;

        if (line >= stdscr->_maxy) break;
    }

    wborder(stdscr, 0, 0, 0, 0, 0, 0, 0, 0);

    wmove(stdscr, 0, 2);
    attron(A_BOLD);
    waddstr(stdscr, " Apache Server | Request Times ");
    attroff(A_BOLD);

    refresh();
    app.is_screen_dirty = false;
}

void listen_input(APP& app) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[255];
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) exit(EXIT_FAILURE);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 2398;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) exit(EXIT_FAILURE);

    listen(sockfd, 5);

    while (!app.quit) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0) exit(EXIT_FAILURE);

        bzero(buffer, 256);
        read(newsockfd, buffer, 255);

        {
            std::lock_guard<std::mutex> guard(app.messages_mutex);

            std::string raw{buffer};
            std::istringstream inbuf(raw);
            double t;
            std::string site_id;
            inbuf >> t >> site_id;

            app.messages.push_back(RequestInfo(t, site_id));
            app.stat.consume(site_id, t);
            app.is_screen_dirty = true;
        }

        write(newsockfd, "OK", 2);
    }
}
