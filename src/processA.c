#include "./../include/processA_utilities.h"
#include <sys/shm.h>
#include <sys/mman.h>
#include <bmpfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#define SEM_PATH_WRITER "/sem_BITMAP_writer"
#define SEM_PATH_READER "/sem_BITMAP_reader"

#define DIAMETER 60
#define WIDTH 1600
#define HEIGHT 600
#define DEPTH 1

FILE *log_file;

// Data structure for storing the bitmap file
bmpfile_t *bmp;

// Shared memory matrix
static uint8_t (*map)[600];

int SIZE = sizeof(uint8_t[WIDTH][HEIGHT]);

void erase_bitmap() { // Erase the whole bitmap (all black)

    rgb_pixel_t black = {0, 0, 0, 0};

    // We erase the existing bitmap
    for (int x = 0; x <= WIDTH; x++) {
        for (int y = 0; y <= HEIGHT; y++) {
            bmp_set_pixel(bmp, x, y, black);
        }
    }
}

void draw_cicle_bitmap() { // Draw a circle on the bitmap

    int radius = DIAMETER/2;
    rgb_pixel_t white = {255, 255, 255, 0};

    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            // If distance is smaller, point is within the circle
            if(sqrt(x*x + y*y) < radius) {                       
                bmp_set_pixel(bmp, circle.x*20 + x, circle.y*20 + y, white); 
            }
        }
    } 
}

void copy_bitmap_to_map() { // Copy values from the bitmap into the shared map

    for (int x = 0; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            rgb_pixel_t *current_pixel = bmp_get_pixel(bmp, x, y);
            map[x][y] = current_pixel->red;
        }
    }
}

int main(int argc, char *argv[]) {

    //Instantiate local bitmap
    bmp = bmp_create(WIDTH, HEIGHT, DEPTH);
    
    // Shared memory map
    const char *shm_name = "/BITMAP";
    int shm_fd;

    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == 1) {
        perror("shared memory segment failed\n"); fflush(stdout);
        exit(1);
    }

    ftruncate(shm_fd, SIZE);
    map = (uint8_t(*)[HEIGHT])mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (map == MAP_FAILED) {
        perror("map failed\n"); fflush(stdout);
        exit(1);
    }

    // Semaphores
    sem_t *sem_id_writer;
    sem_t *sem_id_reader;
    sem_id_writer = sem_open(SEM_PATH_WRITER, O_CREAT, 0644, 1);
    if(sem_id_writer== (void*)-1){
        perror("sem_open failure");
        exit(1);
    }
    sem_id_reader = sem_open(SEM_PATH_READER, O_CREAT, 0644, 1);
    if(sem_id_reader== (void*)-1){
        perror("sem_open failure");
        exit(1);
    }
    sem_init(sem_id_writer, 1, 1);
    sem_init(sem_id_reader, 1, 0);

    // Creating the logfile
    log_file = fopen("./log/processA.log", "w");
    if (log_file == NULL) { 
        perror("error on file opening");
        exit(1); 
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;
    int first_launch = TRUE;

    // Initialize UI
    init_console_ui();

    // Infinite loop
    while (TRUE) {

        // Get input in non-blocking mode
        int cmd = getch();

        // Get the current time to write on the logfile
        time_t clk = time(NULL);
        char * timestamp = ctime(&clk);
        timestamp[24] = ' ';

        // If user resizes screen, re-draw UI...
        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
            }
        }
        // Else, if user presses print button...
        else if(cmd == KEY_MOUSE) {
            if(getmouse(&event) == OK) {
                if(check_button_pressed(print_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Print button pressed");
                    refresh();
                    //sleep(1);

                    for(int j = 0; j < COLS - BTN_SIZE_X - 2; j++) {
                        mvaddch(LINES - 1, j, ' ');

                    // Save image as .bmp file
                    bmp_save(bmp, "./out/snapshot.bmp");

                    }

                fprintf(log_file,"%s- Print button pressed\n", timestamp); fflush(log_file);
                
                }
            }
        }
        // If input is an arrow key, move circle accordingly...
        else if(cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN || first_launch) {
            
            if (first_launch) {
                first_launch = FALSE;
            }

            // Write to the logfile according to the direction of the circle
            if (cmd == KEY_LEFT) {
            fprintf(log_file,"%s- Moving circle with the keyboard to LEFT\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_RIGHT) {
            fprintf(log_file,"%s- Moving circle with the keyboard to RIGHT\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_UP) {
            fprintf(log_file,"%s- Moving circle with the keyboard to UP\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_DOWN) {
            fprintf(log_file,"%s- Moving circle with the keyboard to DOWN\n", timestamp); fflush(log_file);
            }
            
            move_circle(cmd);
            draw_circle();

            erase_bitmap();
            
            draw_cicle_bitmap();

            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Waiting for WRITER semaphore\n", timestamp); fflush(log_file);
            sem_wait(sem_id_writer);
            fprintf(log_file,"%s- WRITER semaphore entered\n", timestamp); fflush(log_file);
            
            copy_bitmap_to_map();
        
            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Leaving the READER semaphore\n", timestamp); fflush(log_file);
            sem_post(sem_id_reader);
            fprintf(log_file,"%s- READER semaphore unlocked\n", timestamp); fflush(log_file);

        }

    }

    // Free resources before termination
    bmp_destroy(bmp);
    close(shm_fd);
    fclose(log_file);
    sem_close(sem_id_reader);
    sem_close(sem_id_writer);
    sem_unlink(SEM_PATH_READER);
    sem_unlink(SEM_PATH_WRITER);

    endwin();
    return 0;
}
