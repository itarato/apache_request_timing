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

struct RequestInfo {
    double t;
    std::string site_id;

    RequestInfo(double _t, std::string _site_id) : t(_t), site_id(_site_id) {};
};

struct APP {
    bool is_screen_dirty;
    bool is_window_dirty;
    bool quit;
    std::vector<RequestInfo> messages;
    std::mutex messages_mutex;

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

    int line = 1;
    for (auto it = app.messages.rbegin(); it != app.messages.rend(); it++) {
        wmove(stdscr, line, 1);

        char t_str[12];
        bzero(t_str, 12);
        sprintf(t_str, "%.2f", it->t);
        attron(A_BOLD);
        waddstr(stdscr, t_str);
        attroff(A_BOLD);

        wmove(stdscr, line, 13);
        waddstr(stdscr, it->site_id.c_str());

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
            app.is_screen_dirty = true;
        }

        write(newsockfd, "OK", 2);
    }
}
