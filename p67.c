#define _XOPEN_SOURCE_EXTENDED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <ctype.h>
#include <regex.h>
#include <math.h>
#include <locale.h>
#include <wchar.h>
#include <ncursesw/curses.h>

#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define ROW_COUNT 2
#define COL_COUNT 3
#define MAX_ROOMS (ROW_COUNT * COL_COUNT)
#define NUM_FLOORS 4
#define MAX_TRAPS 10
#define MAX_FOOD 5
#define INVENTORY_SIZE 5
#define MAX_WEAPONS 5
#define MAX_GOLD 10

typedef struct {
    int x, y;
    int width, height;
    int door_x[2];
    int door_y[2];
} Room;

typedef struct {
    char cells[MAP_HEIGHT][MAP_WIDTH];
    int discovered[MAP_HEIGHT][MAP_WIDTH];
    Room rooms[MAX_ROOMS];
    int room_count;
} Map;

typedef struct {
    int x, y;
    int active;
} Trap;

typedef struct {
    int x, y;
    int value;
    char symbol;
} Food;

typedef struct {
    int x, y;
    int value;
    int collected;
} Gold;

typedef struct {
    int x, y;
    wchar_t symbol;
    char name[20];
    int collected;
} Weapon;

typedef enum { SPELL_HEALTH, SPELL_SPEED, SPELL_DAMAGE } SpellType;

typedef struct {
    SpellType type;
    int active;
    time_t start_time;
    int duration;
} Spell;

#define MAX_SPELLS 5
Spell spells[MAX_SPELLS];
int spell_count = 0;

Weapon weapons[MAX_WEAPONS];
int weapon_count = 0;
int current_weapon = 0;

Gold golds[MAX_GOLD];
int gold_count = 0;
int total_gold = 0;
int show_full_map = 0;
Trap traps[MAX_TRAPS];
Food foods[MAX_FOOD];
int trap_count = 0;
int food_count = 0;
int food_inventory[INVENTORY_SIZE] = {0};

Map floors[NUM_FLOORS];
int current_floor = 0;

int player_x, player_y;
int player_hp = 10;
int player_hunger = 10;

int player_color_pair = 8;

char current_username[50] = "";
char current_password[50] = "";
time_t treasure_enter_time = 0;
time_t healthSpellLastHeal = 0;

typedef struct {
    int current_floor;
    int player_x;
    int player_y;
    int player_hp;
    int player_hunger;
    int total_gold;
    int weapon_count;
    Weapon weapons[MAX_WEAPONS];
    Map floors[NUM_FLOORS];
    int food_inventory[INVENTORY_SIZE];
} SaveData;

typedef struct {
    char username[50];
    int total_gold;
    int games_played;
} ScoreRecord;

void main_menu();
void game_menu();
int new_game();
void last_game();
void game_loop();
void save_game();
int load_game();
void update_scoreboard(const char* username, int gold);
void score_board();
void profile_setting();
int valid_password(const char* password);
int valid_email(const char* emailaddress);
int save_user(const char* username, const char* password, const char* emailaddress);
int sign_up();
int log_in();
void initialize_map(Map *map);
void generate_rooms(Map *map);
void draw_room_on_map(Map *map, Room *r);
void connect_rooms(Map *map);
int bfsPath(Map *map, int sx, int sy, int tx, int ty, int outPath[][2]);
void connectTwoRoomsBFS(Map *map, Room *A, Room *B);
void handle_movement(int ch);
Room create_room_in_grid(int row, int col);
void place_stairs_in_floor(Map *map, int floorIndex);
void handle_stairs(int ch);
void check_traps();
void collect_food();
void show_food_menu();
void game_over();
void collect_gold();
void show_weapon_menu();
void collect_weapon();
void draw_weapons();
void setting_menu();
void collect_spell();
void show_spell_menu();
void update_spell_effects();

int clamp(int val, int min, int max) {
    if(val < min)return min;
    if(val > max)return max;
    return val;
}

void draw_status_bars() {
    wchar_t heart[3] = {0x2764, 0xFE0F, 0};
    mvprintw(0,0,"HP: [");
    attron(COLOR_PAIR(3));
    for(int i=0;i<10;i++){
        if(i<player_hp)
            addwstr(heart);
        else
            printw(" ");
    }
    attroff(COLOR_PAIR(3));
    printw("]");
    mvprintw(1,0,"Hunger: [");
    attron(COLOR_PAIR(4));
    wchar_t burger[3] = {0x1F354,0,0};
    for(int i=0;i<10;i++){
        if(i<player_hunger)
            addwstr(burger);
        else
            printw(" ");
    }
    attroff(COLOR_PAIR(4));
    printw("]");
    mvprintw(2,0,"Gold: %d  Floor: %d", total_gold, current_floor+1);
}

void save_game() {
    FILE *f = fopen("savegame.dat","wb");
    if(!f)return;
    SaveData sd;
    sd.current_floor = current_floor;
    sd.player_x = player_x;
    sd.player_y = player_y;
    sd.player_hp = player_hp;
    sd.player_hunger = player_hunger;
    sd.total_gold = total_gold;
    sd.weapon_count = weapon_count;
    memcpy(sd.weapons,weapons,sizeof(Weapon)*MAX_WEAPONS);
    memcpy(sd.floors,floors,sizeof(floors));
    memcpy(sd.food_inventory,food_inventory,sizeof(food_inventory));
    fwrite(&sd,sizeof(SaveData),1,f);
    fclose(f);
}

int load_game() {
    FILE *f = fopen("savegame.dat","rb");
    if(!f)return 0;
    SaveData sd;
    fread(&sd,sizeof(SaveData),1,f);
    fclose(f);
    current_floor = sd.current_floor;
    player_x = sd.player_x;
    player_y = sd.player_y;
    player_hp = sd.player_hp;
    player_hunger = sd.player_hunger;
    total_gold = sd.total_gold;
    weapon_count = sd.weapon_count;
    memcpy(weapons,sd.weapons,sizeof(Weapon)*MAX_WEAPONS);
    memcpy(floors,sd.floors,sizeof(floors));
    memcpy(food_inventory,sd.food_inventory,sizeof(food_inventory));
    return 1;
}

void update_scoreboard(const char *username, int gold) {
    FILE *f = fopen("scoreboard.txt","r");
    ScoreRecord records[100];
    int count = 0, foundIndex = -1;
    if(f){
        while(fscanf(f,"%49[^,],%d,%d\n",records[count].username,&records[count].total_gold,&records[count].games_played)==3){
            if(strcmp(records[count].username,username)==0)
                foundIndex = count;
            count++;
        }
        fclose(f);
    }
    if(foundIndex>=0){
        records[foundIndex].total_gold += gold;
        records[foundIndex].games_played += 1;
    } else {
        strcpy(records[count].username,username);
        records[count].total_gold = gold;
        records[count].games_played = 1;
        count++;
    }
    f = fopen("scoreboard.txt","w");
    for(int i=0;i<count;i++){
        fprintf(f,"%s,%d,%d\n",records[i].username,records[i].total_gold,records[i].games_played);
    }
    fclose(f);
}

int cmpScore(const void *a, const void *b) {
    ScoreRecord *ra = (ScoreRecord*)a;
    ScoreRecord *rb = (ScoreRecord*)b;
    return rb->total_gold - ra->total_gold;
}

void score_board() {
    FILE *f = fopen("scoreboard.txt","r");
    if(!f){
        clear();
        mvprintw(0,0,"No scoreboard data available.");
        refresh();
        getch();
        return;
    }
    ScoreRecord records[100];
    int count = 0;
    while(fscanf(f,"%49[^,],%d,%d\n",records[count].username,&records[count].total_gold,&records[count].games_played)==3){
        count++;
    }
    fclose(f);
    qsort(records,count,sizeof(ScoreRecord),cmpScore);
    clear();
    mvprintw(1,2,"Rank   Username                Total Gold   Games Played");
    for(int i=0;i<count && i<10;i++){
        int rank = i+1;
        if(i==0){
            attron(COLOR_PAIR(5));
            mvprintw(3+i,2,"%2d.  %s (🥇)   %10d   %5d",rank,records[i].username,records[i].total_gold,records[i].games_played);
            attroff(COLOR_PAIR(5));
        } else if(i==1){
            attron(COLOR_PAIR(2));
            mvprintw(3+i,2,"%2d.  %s (🥈)     %10d   %5d",rank,records[i].username,records[i].total_gold,records[i].games_played);
            attroff(COLOR_PAIR(2));
        } else if(i==2){
            attron(COLOR_PAIR(3));
            mvprintw(3+i,2,"%2d.  %s (🥉) %10d   %5d",rank,records[i].username,records[i].total_gold,records[i].games_played);
            attroff(COLOR_PAIR(3));
        } else {
            mvprintw(3+i,2,"%2d.  %s                 %10d   %5d",rank,records[i].username,records[i].total_gold,records[i].games_played);
        }
    }
    mvprintw(15,2,"Press any key to return to menu.");
    refresh();
    getch();
}

void profile_setting() {
    clear();
    mvprintw(5,10,"**Profile Settings**");
    mvprintw(7,10,"Username: %s",current_username);
    mvprintw(8,10,"Password: %s",current_password);
    mvprintw(10,10,"Press any key to return to menu.");
    refresh();
    getch();
}

int sign_up() {
    char user[50], pass[50], mail[50];
    clear();
    mvprintw(5,23,"**Sign up page** (Press 'q' to cancel)");
    echo();
    mvprintw(7,25,"Username:");
    getstr(user);
    if(user[0]=='q' || user[0]=='Q'){ noecho(); return 0; }
    mvprintw(10,25,"Password:");
    getstr(pass);
    mvprintw(13,25,"Email:");
    getstr(mail);
    noecho();
    if(strlen(user)==0 || strlen(pass)==0 || strlen(mail)==0){
        clear();
        mvprintw(0,0,"All fields are required! Please try again.");
        refresh();
        getch();
        return 0;
    }
    if(!valid_password(pass)){
        clear();
        mvprintw(0,0,"Invalid password! Must be >=7 chars with upper, lower and digit.");
        refresh();
        getch();
        return 0;
    }
    if(!valid_email(mail)){
        clear();
        mvprintw(0,0,"Invalid email!");
        refresh();
        getch();
        return 0;
    }
    if(save_user(user,pass,mail)==0){
        mvprintw(0,0,"User saved successfully!");
        strcpy(current_username,user);
        strcpy(current_password,pass);
    } else {
        mvprintw(0,0,"Error saving user data!");
        refresh();
        getch();
        return 0;
    }
    refresh();
    getch();
    return 1;
}

int log_in() {
    char user[50], pass[50];
    char su[50], sp[50], se[50];
    int found = 0, passok = 0;
    clear();
    mvprintw(5,23,"**Login page** (Press 'q' to cancel)");
    echo();
    mvprintw(7,25,"Username:");
    getstr(user);
    if(user[0]=='q' || user[0]=='Q'){ noecho(); return 0; }
    mvprintw(10,25,"Password:");
    getstr(pass);
    noecho();
    if(!valid_password(pass)){
        clear();
        mvprintw(0,0,"Invalid password format!");
        refresh();
        getch();
        return 0;
    }
    FILE *f = fopen("users.txt","r");
    if(!f){
        mvprintw(0,0,"Could not open user file!");
        refresh();
        getch();
        return 0;
    }
    while(fscanf(f,"%49[^,],%49[^,],%49[^\n]\n",su,sp,se)!=EOF){
        if(strcmp(su,user)==0){
            found = 1;
            if(strcmp(sp,pass)==0)
                passok = 1;
            break;
        }
    }
    fclose(f);
    if(!found){
        clear();
        mvprintw(0,0,"User not found!");
        refresh();
        getch();
        return 0;
    }
    if(!passok){
        clear();
        mvprintw(0,0,"Wrong password!");
        refresh();
        getch();
        return 0;
    }
    strcpy(current_username,user);
    strcpy(current_password,pass);
    mvprintw(0,0,"Login successful!");
    refresh();
    getch();
    return 1;
}

void initialize_map(Map *map) {
    int i,j;
    for(i=0;i<MAP_HEIGHT;i++){
        for(j=0;j<MAP_WIDTH;j++){
            map->cells[i][j] = ' ';
            map->discovered[i][j] = 0;
        }
    }
    map->room_count = 0;
}

Room create_room_in_grid(int row, int col) {
    int section_w = MAP_WIDTH / COL_COUNT;
    int section_h = MAP_HEIGHT / ROW_COUNT;
    int start_x = col * section_w;
    int start_y = (row==0)?3:row*section_h;
    int is_room6 = (row==ROW_COUNT-1)&&(col==COL_COUNT-1);
    Room r;
    if(is_room6){
        int fixed_w = 8, fixed_h = 8;
        r.x = start_x + (section_w - fixed_w)/2;
        r.y = start_y + (section_h - fixed_h)/2;
        r.width = fixed_w;
        r.height = fixed_h;
    } else {
        int min_w = 5, min_h = 5;
        int max_w = section_w - 2, max_h = section_h - 2;
        if(max_w < min_w) max_w = min_w;
        if(max_h < min_h) max_h = min_h;
        r.width = min_w + rand()%(max_w-min_w+1);
        r.height = min_h + rand()%(max_h-min_h+1);
        r.x = start_x + rand()%(section_w-r.width);
        r.y = start_y + rand()%(section_h-r.height);
    }
    r.door_x[0] = r.x + r.width - 1;
    r.door_y[0] = r.y + 1 + rand()%(r.height-2);
    r.door_x[1] = r.x + 1 + rand()%(r.width-2);
    r.door_y[1] = r.y + r.height - 1;
    return r;
}

void draw_room_on_map(Map *map, Room *r) {
    int yy, xx, i;
    for(yy = r->y; yy < r->y + r->height; yy++){
        for(xx = r->x; xx < r->x + r->width; xx++){
            if(yy==r->y || yy==(r->y+r->height-1))
                map->cells[yy][xx] = '-';
            else if(xx==r->x || xx==(r->x+r->width-1))
                map->cells[yy][xx] = '|';
            else
                map->cells[yy][xx] = '.';
        }
    }
    for(i=0;i<2;i++){
        int dx = r->door_x[i], dy = r->door_y[i];
        map->cells[dy][dx] = '+';
        if(dx==(r->x+r->width-1))
            if(dx-1>=0) map->cells[dy][dx-1] = '.';
        if(dy==(r->y+r->height-1))
            if(dy-1>=0) map->cells[dy-1][dx] = '.';
    }
    int num_items = 3 + rand()%5, i_try, d;
    for(i=0;i<num_items;i++){
        i_try = 0;
        while(i_try < 100){
            i_try++;
            xx = r->x+1+rand()%(r->width-2);
            yy = r->y+1+rand()%(r->height-2);
            int near_door = 0;
            for(d=0; d<2; d++){
                if(abs(xx - r->door_x[d])<2 || abs(yy - r->door_y[d])<2){
                    near_door = 1;
                    break;
                }
            }
            if(near_door) continue;
            if(map->cells[yy][xx] != '.') continue;
            int item_type = rand()%100;
            if(item_type<10)
                map->cells[yy][xx] = 'O';
            else if(item_type<50)
                map->cells[yy][xx] = '^';
            else if(item_type<65)
                map->cells[yy][xx] = 'F';
            else if(item_type<70)
                map->cells[yy][xx] = 'f';
            else if(item_type<80)
                map->cells[yy][xx] = 'S';
            else if(item_type<95)
                map->cells[yy][xx] = 'G';
            else
                map->cells[yy][xx] = 'K';
            break;
        }
    }
}

void generate_rooms(Map *map) {
    int idx = 0, rr, cc;
    map->room_count = 0;
    for(rr=0; rr<ROW_COUNT; rr++){
        for(cc=0; cc<COL_COUNT; cc++){
            Room rm = create_room_in_grid(rr,cc);
            map->rooms[idx++] = rm;
            map->room_count++;
        }
    }
    for(idx=0; idx<map->room_count; idx++){
        draw_room_on_map(map,&map->rooms[idx]);
    }
}

#define QUEUE_SIZE (MAP_WIDTH * MAP_HEIGHT)
typedef struct {
    int x, y;
} BFSNode;
static int inRange(int x, int y) {
    return (x>=0 && x<MAP_WIDTH && y>=0 && y<MAP_HEIGHT);
}
static const int DIR[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
int bfsPath(Map *map, int sx, int sy, int tx, int ty, int outPath[][2]) {
    if(sx==tx && sy==ty){
        outPath[0][0]=sx;
        outPath[0][1]=sy;
        return 1;
    }
    static int visited[MAP_HEIGHT][MAP_WIDTH];
    static int parentX[MAP_HEIGHT][MAP_WIDTH];
    static int parentY[MAP_HEIGHT][MAP_WIDTH];
    memset(visited,0,sizeof(visited));
    int i,j;
    for(i=0;i<MAP_HEIGHT;i++){
        for(j=0;j<MAP_WIDTH;j++){
            parentX[i][j] = -1;
            parentY[i][j] = -1;
        }
    }
    BFSNode queue[QUEUE_SIZE];
    int front=0, back=0;
    visited[sy][sx] = 1;
    queue[back].x = sx; queue[back].y = sy; back++;
    while(front<back){
        BFSNode cur = queue[front++];
        int cx = cur.x, cy = cur.y;
        if(cx==tx && cy==ty){
            int length=0, px=tx, py=ty;
            while(px>=0 && py>=0){
                outPath[length][0] = px;
                outPath[length][1] = py;
                length++;
                int nx = parentX[py][px];
                int ny = parentY[py][px];
                px = nx; py = ny;
                if(px<0 || py<0) break;
            }
            for(i=0;i<length/2;i++){
                int opp = length-1-i;
                int tmpx = outPath[i][0], tmpy = outPath[i][1];
                outPath[i][0] = outPath[opp][0];
                outPath[i][1] = outPath[opp][1];
                outPath[opp][0] = tmpx;
                outPath[opp][1] = tmpy;
            }
            return length;
        }
        for(i=0;i<4;i++){
            int nx = cx + DIR[i][0];
            int ny = cy + DIR[i][1];
            if(inRange(nx,ny) && !visited[ny][nx]){
                if(map->cells[ny][nx]==' '){
                    visited[ny][nx] = 1;
                    parentX[ny][nx] = cx;
                    parentY[ny][nx] = cy;
                    queue[back].x = nx;
                    queue[back].y = ny;
                    back++;
                }
            }
        }
    }
    return -1;
}

static void get_outside_door(Room *r, int doorIndex, int *ox, int *oy) {
    int dx = r->door_x[doorIndex], dy = r->door_y[doorIndex];
    *ox = dx; *oy = dy;
    if(dx==(r->x+r->width-1))
        (*ox)++;
    if(dy==(r->y+r->height-1))
        (*oy)++;
}

void connectTwoRoomsBFS(Map *map, Room *A, Room *B) {
    int bestLen = 999999, pathLen = 0;
    static int bestPath[MAP_WIDTH * MAP_HEIGHT][2];
    int i,j;
    for(i=0;i<2;i++){
        int outAx, outAy;
        get_outside_door(A,i,&outAx,&outAy);
        for(j=0;j<2;j++){
            int outBx, outBy;
            get_outside_door(B,j,&outBx,&outBy);
            static int tempPath[MAP_WIDTH * MAP_HEIGHT][2];
            int length = bfsPath(map,outAx,outAy,outBx,outBy,tempPath);
            if(length>0 && length<bestLen){
                bestLen = length;
                memcpy(bestPath,tempPath,length*sizeof(bestPath[0]));
                pathLen = length;
            }
        }
    }
    for(i=0;i<pathLen;i++){
        int xx = bestPath[i][0], yy = bestPath[i][1];
        if(map->cells[yy][xx]==' ')
            map->cells[yy][xx] = '#';
    }
}

void connect_rooms(Map *map) {
    int idx = 0, rr, cc;
    for(rr=0; rr<ROW_COUNT; rr++){
        for(cc=0; cc<COL_COUNT; cc++){
            int i = idx++;
            if(cc<COL_COUNT-1)
                connectTwoRoomsBFS(map,&map->rooms[i],&map->rooms[i+1]);
            if(rr<ROW_COUNT-1)
                connectTwoRoomsBFS(map,&map->rooms[i],&map->rooms[i+COL_COUNT]);
        }
    }
}

void place_stairs_in_floor(Map *map, int floorIndex) {
    if(floorIndex<NUM_FLOORS-1){
        Room *stair_room = &map->rooms[5];
        int stair_x = stair_room->x + stair_room->width/2;
        int stair_y = stair_room->y + stair_room->height/2;
        map->cells[stair_y][stair_x] = '>';
    } else {
        Room *stair_room = &map->rooms[5];
        int stair_x = stair_room->x + stair_room->width/2;
        int stair_y = stair_room->y + stair_room->height/2;
        map->cells[stair_y][stair_x] = '>';
        Room *treasure_room = &map->rooms[0];
        connectTwoRoomsBFS(map,treasure_room,&map->rooms[1]);
        connectTwoRoomsBFS(map,treasure_room,&map->rooms[3]);
    }
}

void handle_stairs(int ch) {
    Map *map = &floors[current_floor];
    if(ch=='>' && map->cells[player_y][player_x]=='>'){
        if(current_floor<NUM_FLOORS-1){
            current_floor++;
            Room *stair_room = &floors[current_floor].rooms[5];
            player_x = stair_room->x + stair_room->width/2;
            player_y = stair_room->y + stair_room->height/2;
        }
        return;
    }
}

void collect_spell() {
    Map *map = &floors[current_floor];
    if(map->cells[player_y][player_x]=='S'){
        if(spell_count<MAX_SPELLS){
            Spell newSpell;
            newSpell.type = rand()%3;
            newSpell.active = 0;
            newSpell.start_time = 0;
            newSpell.duration = 10;
            spells[spell_count++] = newSpell;
            mvprintw(1,0,"Collected a spell!");
        }
        map->cells[player_y][player_x] = '.';
        refresh();
        getch();
    }
}

void show_spell_menu() {
    clear();
    mvprintw(0,0,"Spell Inventory (Press 1-%d to activate, Q to quit):", spell_count);
    int i;
    for(i=0;i<spell_count;i++){
        char *spellName;
        wchar_t spellIcon[4];
        switch(spells[i].type){
            case SPELL_HEALTH: spellName="Health"; wcscpy(spellIcon,L"\U0001F489"); break;
            case SPELL_SPEED: spellName="Speed"; wcscpy(spellIcon,L"\u26A1"); break;
            case SPELL_DAMAGE: spellName="Damage"; wcscpy(spellIcon,L"\U0001F9EA"); break;
            default: spellName="Unknown"; wcscpy(spellIcon,L"?"); break;
        }
        if(spells[i].active){
            int remaining = (int)(spells[i].duration - (time(NULL)-spells[i].start_time));
            mvprintw(2+i,2,"%d: %s (%ls) Active, %d sec remaining", i+1, spellName, spellIcon, remaining);
        } else {
            mvprintw(2+i,2,"%d: %s (%ls)", i+1, spellName, spellIcon);
        }
    }
    refresh();
    int ch = getch();
    if(ch>='1' && ch<'1'+spell_count){
        int idx = ch-'1';
        if(!spells[idx].active){
            spells[idx].active = 1;
            spells[idx].start_time = time(NULL);
            if(spells[idx].type==SPELL_HEALTH)
                healthSpellLastHeal = time(NULL);
        }
    }
}

void update_spell_effects() {
    time_t now = time(NULL);
    int i;
    for(i=0;i<spell_count;i++){
        if(spells[i].active){
            if(now-spells[i].start_time>=spells[i].duration)
                spells[i].active = 0;
            if(spells[i].active && spells[i].type==SPELL_HEALTH){
                if(player_hunger>5 && now-healthSpellLastHeal>=5){
                    if(player_hp<10)
                        player_hp++;
                    healthSpellLastHeal = now;
                }
            }
        }
    }
}

void game_loop() {
    time_t last_hunger_update = time(NULL);
    time_t last_hp_loss_time = time(NULL);
    time_t last_hp_gain_time = time(NULL);
    while(1){
        clear();
        draw_status_bars();
        if(current_floor==NUM_FLOORS-1){
            Room *treasure_room = &floors[current_floor].rooms[0];
            if(player_x>=treasure_room->x && player_x<treasure_room->x+treasure_room->width &&
               player_y>=treasure_room->y && player_y<treasure_room->y+treasure_room->height){
                if(treasure_enter_time==0)
                    treasure_enter_time = time(NULL);
                else if(time(NULL)-treasure_enter_time>=10){
                    update_scoreboard(current_username,total_gold);
                    clear();
                    mvprintw(10,10,"YOU FOUND THE TREASURE! Total Gold: %d", total_gold);
                    refresh();
                    getch();
                    endwin();
                    exit(0);
                } else {
                    int remaining = 10 - (time(NULL)-treasure_enter_time);
                    mvprintw(0,50,"Ends in: %d sec", remaining);
                }
            } else {
                treasure_enter_time = 0;
            }
        }
        Map *map = &floors[current_floor];
        int i,j;
        for(i=3;i<MAP_HEIGHT;i++){
            for(j=0;j<MAP_WIDTH;j++){
                char c = map->cells[i][j];
                if(show_full_map || map->discovered[i][j]){
                    switch(c){
                        case '^': attron(COLOR_PAIR(3)); mvaddch(i,j,c); attroff(COLOR_PAIR(3)); break;
                        case 'F': { wchar_t foodSymbol[2] = {L'🍎',L'\0'}; attron(COLOR_PAIR(4)); mvaddwstr(i,j,foodSymbol); attroff(COLOR_PAIR(4)); break; }
                        case 'f': { wchar_t foodSymbol[2] = {L'🍔',L'\0'}; attron(COLOR_PAIR(4)); mvaddwstr(i,j,foodSymbol); attroff(COLOR_PAIR(4)); break; }
                        case 'G': { wchar_t goldSymbol[2] = {L'💰',L'\0'}; attron(COLOR_PAIR(5)); mvaddwstr(i,j,goldSymbol); attroff(COLOR_PAIR(5)); break; }
                        case 'K': { wchar_t blackGoldSymbol[2] = {L'💎',L'\0'}; attron(COLOR_PAIR(5)); mvaddwstr(i,j,blackGoldSymbol); attroff(COLOR_PAIR(5)); break; }
                        case 'S': { wchar_t spellSymbol[2] = {L'S',L'\0'}; attron(COLOR_PAIR(7)); mvaddwstr(i,j,spellSymbol); attroff(COLOR_PAIR(7)); break; }
                        case '>': case '=': attron(COLOR_PAIR(6)); mvaddch(i,j,c); attroff(COLOR_PAIR(6)); break;
                        default: mvaddch(i,j,c);
                    }
                } else {
                    mvaddch(i,j,' ');
                }
            }
        }
        draw_weapons();
        { wchar_t playerSymbol[2] = {L'☺',L'\0'}; attron(COLOR_PAIR(player_color_pair)|A_BOLD); mvaddwstr(player_y,player_x,playerSymbol); attroff(COLOR_PAIR(player_color_pair)|A_BOLD); }
        update_spell_effects();
        int ch = getch();
        if(ch=='q' || ch=='Q'){
            mvprintw(2,50,"Save game before exit? (y/n)");
            refresh();
            int c = getch();
            if(c=='y' || c=='Y')
                save_game();
            endwin();
            exit(0);
        }
        if(ch=='m' || ch=='M')
            show_full_map = !show_full_map;
        if(ch=='i' || ch=='I')
            show_weapon_menu();
        if(ch=='p' || ch=='P')
            profile_setting();
        if(ch=='t' || ch=='T')
            show_spell_menu();
        handle_movement(ch);
        check_traps();
        collect_food();
        collect_gold();
        collect_weapon();
        collect_spell();
        if(ch=='>')
            handle_stairs(ch);
        if(ch=='e' || ch=='E')
            show_food_menu();
        refresh();
    }
}

int new_game() {
    srand((unsigned)time(NULL));
    total_gold = 0;
    player_hp = 10;
    player_hunger = 10;
    current_floor = 0;
    weapon_count = 0;
    current_weapon = 0;
    treasure_enter_time = 0;
    spell_count = 0;
    weapons[0].symbol = L'M';
    strcpy(weapons[0].name,"Mace");
    weapons[0].collected = 1;
    weapon_count = 1;
    int f,i,tries,wx,wy;
    for(f=0; f<NUM_FLOORS; f++){
        initialize_map(&floors[f]);
        generate_rooms(&floors[f]);
        connect_rooms(&floors[f]);
        for(i=0;i<floors[f].room_count;i++){
            draw_room_on_map(&floors[f],&floors[f].rooms[i]);
        }
        place_stairs_in_floor(&floors[f],f);
        for(i=0;i<2;i++){
            if(weapon_count>=MAX_WEAPONS) break;
            Room *r = &floors[f].rooms[rand()%MAX_ROOMS];
            tries = 0;
            while(tries<100){
                wx = r->x+1+rand()%(r->width-2);
                wy = r->y+1+rand()%(r->height-2);
                if(wx>=r->x+1 && wx<r->x+r->width-1 && wy>=r->y+1 && wy<r->y+r->height-1 && floors[f].cells[wy][wx]=='.')
                    break;
                tries++;
            }
            if(tries>=100) continue;
            weapons[weapon_count].x = wx;
            weapons[weapon_count].y = wy;
            weapons[weapon_count].collected = 0;
            int type = rand()%4;
            switch(type){
                case 0: weapons[weapon_count].symbol = L'⚔'; strcpy(weapons[weapon_count].name,"Sword"); break;
                case 1: weapons[weapon_count].symbol = L'🗡'; strcpy(weapons[weapon_count].name,"Dagger"); break;
                case 2: weapons[weapon_count].symbol = L'🜂'; strcpy(weapons[weapon_count].name,"Wand"); break;
                case 3: weapons[weapon_count].symbol = L'🏹'; strcpy(weapons[weapon_count].name,"Arrow"); break;
            }
            weapon_count++;
        }
    }
    player_x = floors[0].rooms[0].door_x[0];
    player_y = floors[0].rooms[0].door_y[0];
    game_loop();
    return 1;
}

void last_game() {
    if(!load_game()){
        clear();
        mvprintw(0,0,"No saved game found!");
        refresh();
        getch();
        return;
    }
    game_loop();
}

void handle_movement(int ch) {
    Map *map = &floors[current_floor];
    int dx,dy,xx,yy,i;
    for(dy=-5; dy<=5; dy++){
        for(dx=-5; dx<=5; dx++){
            xx = player_x+dx;
            yy = player_y+dy;
            if(xx>=0 && xx<MAP_WIDTH && yy>=0 && yy<MAP_HEIGHT)
                map->discovered[yy][xx] = 1;
        }
    }
    int nx = player_x, ny = player_y;
    int speedMultiplier = 1;
    for(i=0;i<spell_count;i++){
        if(spells[i].active && spells[i].type==SPELL_SPEED){
            speedMultiplier = 2;
            break;
        }
    }
    switch(ch){
        case KEY_UP: ny -= speedMultiplier; break;
        case KEY_DOWN: ny += speedMultiplier; break;
        case KEY_LEFT: nx -= speedMultiplier; break;
        case KEY_RIGHT: nx += speedMultiplier; break;
        case 'q': nx -= speedMultiplier; ny -= speedMultiplier; break;
        case 'e': nx += speedMultiplier; ny -= speedMultiplier; break;
        case 'z': nx -= speedMultiplier; ny += speedMultiplier; break;
        case 'c': nx += speedMultiplier; ny += speedMultiplier; break;
        default: break;
    }
    if(nx>=0 && nx<MAP_WIDTH && ny>=0 && ny<MAP_HEIGHT){
        char c = map->cells[ny][nx];
        if(c=='.' || c=='#' || c=='+' || c=='>' || c=='=' || c=='F' || c=='f' || c=='G' || c=='K' || c=='^' || c=='S'){
            player_x = nx;
            player_y = ny;
        }
    }
}

void check_traps() {
    Map *map = &floors[current_floor];
    if(map->cells[player_y][player_x]=='^'){
        player_hp = clamp(player_hp-1,0,10);
        map->cells[player_y][player_x] = '.';
        mvprintw(1,0,"Trap activated! -1 HP!");
        refresh();
        getch();
    }
}

void collect_food() {
    Map *map = &floors[current_floor];
    char cell = map->cells[player_y][player_x];
    int i;
    if(cell=='F' || cell=='f'){
        for(i=0;i<INVENTORY_SIZE;i++){
            if(food_inventory[i]==0){
                food_inventory[i] = 1;
                map->cells[player_y][player_x] = '.';
                mvprintw(1,0,"Food collected! Added to slot %d", i+1);
                refresh();
                getch();
                return;
            }
        }
        mvprintw(1,0,"Inventory full! Can't collect food.");
        refresh();
        getch();
    }
}

void collect_gold() {
    Map *map = &floors[current_floor];
    char cell = map->cells[player_y][player_x];
    if(cell=='G'){
        total_gold += 1;
        map->cells[player_y][player_x] = '.';
        mvprintw(1,0,"Gold collected! Total: %d", total_gold);
        refresh();
        getch();
    } else if(cell=='K'){
        total_gold += 3;
        map->cells[player_y][player_x] = '.';
        mvprintw(1,0,"Black Gold collected! Total: %d", total_gold);
        refresh();
        getch();
    }
}

void show_food_menu() {
    clear();
    int i;
    mvprintw(0,0,"Food Inventory (Press 1-5 to use, Q to quit):");
    for(i=0;i<INVENTORY_SIZE;i++){
        if(food_inventory[i]!=0)
            mvprintw(i+2,2,"%d: Food (+%d Hunger)", i+1, food_inventory[i]);
        else
            mvprintw(i+2,2,"%d: Empty", i+1);
    }
    while(1){
        int ch = getch();
        if(ch=='q' || ch=='Q') break;
        if(ch>='1' && ch<='5'){
            int idx = ch-'1';
            if(food_inventory[idx]!=0){
                player_hunger = clamp(player_hunger+food_inventory[idx],0,10);
                mvprintw(10,0,"Used food! Hunger increased. Current Hunger: %d", player_hunger);
                food_inventory[idx] = 0;
                refresh();
                getch();
                break;
            }
        }
    }
}

void collect_weapon() {
    int i;
    for(i=0;i<weapon_count;i++){
        if(!weapons[i].collected && player_x==weapons[i].x && player_y==weapons[i].y){
            weapons[i].collected = 1;
            mvprintw(1,0,"Collected %s!", weapons[i].name);
            refresh();
            getch();
        }
    }
}

void show_weapon_menu() {
    clear();
    int i;
    mvprintw(0,0,"Weapons (Press 1-%d to switch, Q to quit):", weapon_count);
    for(i=0;i<weapon_count;i++){
        if(weapons[i].collected){
            mvprintw(i+2,2,"%d: %s (%lc) %s", i+1, weapons[i].name, weapons[i].symbol, (i==current_weapon)?"[Equipped]":"");
        }
    }
    refresh();
    while(1){
        int ch = getch();
        if(ch=='q' || ch=='Q') break;
        if(ch>='1' && ch<'1'+weapon_count){
            int idx = ch-'1';
            if(idx<weapon_count && weapons[idx].collected){
                current_weapon = idx;
                mvprintw(10,0,"Equipped %s!", weapons[idx].name);
                refresh();
                getch();
                break;
            }
        }
    }
}

void draw_weapons() {
    Map *map = &floors[current_floor];
    int i;
    for(i=0;i<weapon_count;i++){
        if(!weapons[i].collected && map->discovered[weapons[i].y][weapons[i].x]){
            wchar_t wstr[2] = {weapons[i].symbol, L'\0'};
            attron(COLOR_PAIR(7));
            mvaddwstr(weapons[i].y, weapons[i].x, wstr);
            attroff(COLOR_PAIR(7));
        }
    }
}

void game_over() {
    clear();
    mvprintw(10,10,"GAME OVER! Your HP reached 0.");
    refresh();
    getch();
    endwin();
    exit(0);
}

void setting_menu() {
    int choice = 0, ch, i;
    const char *opts[] = {"Change Player Color", "Back"};
    int total = 2;
    while(1){
        clear();
        mvprintw(3,23,"**Settings Menu**");
        for(i=0;i<total;i++){
            if(i==choice){
                attron(COLOR_PAIR(1));
                mvprintw(7+i*2,25,"%s",opts[i]);
                attroff(COLOR_PAIR(1));
            } else {
                attron(COLOR_PAIR(2));
                mvprintw(7+i*2,25,"%s",opts[i]);
                attroff(COLOR_PAIR(2));
            }
        }
        ch = getch();
        if(ch==KEY_UP)
            choice = (choice==0)?(total-1):(choice-1);
        else if(ch==KEY_DOWN)
            choice = (choice==(total-1))?0:(choice+1);
        else if(ch=='\n'){
            if(choice==0){
                int colorChoice = 0, c, totalColors = 7;
                const char *colorOpts[] = {"White","Red","Green","Yellow","Blue","Magenta","Cyan"};
                while(1){
                    clear();
                    mvprintw(3,23,"**Select Player Color**");
                    for(i=0;i<totalColors;i++){
                        if(i==colorChoice){
                            attron(COLOR_PAIR(1));
                            mvprintw(7+i*2,25,"%s",colorOpts[i]);
                            attroff(COLOR_PAIR(1));
                        } else {
                            attron(COLOR_PAIR(2));
                            mvprintw(7+i*2,25,"%s",colorOpts[i]);
                            attroff(COLOR_PAIR(2));
                        }
                    }
                    c = getch();
                    if(c==KEY_UP)
                        colorChoice = (colorChoice==0)?(totalColors-1):(colorChoice-1);
                    else if(c==KEY_DOWN)
                        colorChoice = (colorChoice==(totalColors-1))?0:(colorChoice+1);
                    else if(c=='\n'){
                        int chosenColor;
                        switch(colorChoice){
                            case 0: chosenColor = COLOR_WHITE; break;
                            case 1: chosenColor = COLOR_RED; break;
                            case 2: chosenColor = COLOR_GREEN; break;
                            case 3: chosenColor = COLOR_YELLOW; break;
                            case 4: chosenColor = COLOR_BLUE; break;
                            case 5: chosenColor = COLOR_MAGENTA; break;
                            case 6: chosenColor = COLOR_CYAN; break;
                            default: chosenColor = COLOR_WHITE;
                        }
                        init_pair(8,chosenColor,COLOR_BLACK);
                        player_color_pair = 8;
                        break;
                    }
                    else if(c=='q' || c=='Q')
                        break;
                }
            } else if(choice==1)
                break;
        }
    }
}

void game_menu() {
    int choice = 0, total = 6, ch, i;
    const char *opts[] = {"Create a new game","Continue the previous game","Scoreboard","Profile settings","Setting menu","Back"};
    start_color();
    init_pair(1,COLOR_BLACK,COLOR_GREEN);
    init_pair(2,COLOR_WHITE,COLOR_BLACK);
    while(1){
        clear();
        mvprintw(3,23,"**Game menu**");
        for(i=0;i<total;i++){
            if(i==choice){
                attron(COLOR_PAIR(1));
                mvprintw(7+i*2,25,"%s",opts[i]);
                attroff(COLOR_PAIR(1));
            } else {
                attron(COLOR_PAIR(2));
                mvprintw(7+i*2,25,"%s",opts[i]);
                attroff(COLOR_PAIR(2));
            }
        }
        ch = getch();
        if(ch==KEY_UP)
            choice = (choice==0)?(total-1):(choice-1);
        else if(ch==KEY_DOWN)
            choice = (choice==(total-1))?0:(choice+1);
        else if(ch=='\n'){
            if(choice==0)
                new_game();
            else if(choice==1)
                last_game();
            else if(choice==2)
                score_board();
            else if(choice==3)
                profile_setting();
            else if(choice==4)
                setting_menu();
            else if(choice==5)
                break;
        }
    }
    endwin();
}

void main_menu() {
    int choice = 0, n = 3, ch, i;
    const char *opts[] = {"Login","Signup","Exit"};
    setlocale(LC_ALL,"");
    initscr();
    cbreak();
    keypad(stdscr,TRUE);
    noecho();
    if(!has_colors()){
        endwin();
        printf("No color support.\n");
        return;
    }
    start_color();
    init_pair(1,COLOR_BLACK,COLOR_GREEN);
    init_pair(2,COLOR_WHITE,COLOR_BLACK);
    init_pair(3,COLOR_RED,COLOR_BLACK);
    init_pair(4,COLOR_GREEN,COLOR_BLACK);
    init_pair(5,COLOR_YELLOW,COLOR_BLACK);
    init_pair(6,COLOR_BLUE,COLOR_BLACK);
    init_pair(7,COLOR_CYAN,COLOR_BLACK);
    init_pair(8,COLOR_WHITE,COLOR_BLACK);
    while(1){
        clear();
        mvprintw(3,23,"**Main menu**");
        for(i=0;i<n;i++){
            if(i==choice){
                attron(COLOR_PAIR(1));
                mvprintw(7+i*2,25,"%s",opts[i]);
                attroff(COLOR_PAIR(1));
            } else {
                attron(COLOR_PAIR(2));
                mvprintw(7+i*2,25,"%s",opts[i]);
                attroff(COLOR_PAIR(2));
            }
        }
        ch = getch();
        if(ch==KEY_UP)
            choice = (choice==0)?(n-1):(choice-1);
        else if(ch==KEY_DOWN)
            choice = (choice==(n-1))?0:(choice+1);
        else if(ch=='\n'){
            if(choice==0){
                if(log_in())
                    game_menu();
            }
            else if(choice==1){
                if(sign_up())
                    game_menu();
            }
            else if(choice==2)
                break;
        }
    }
    endwin();
}

int valid_password(const char* pass) {
    int up = 0, lo = 0, di = 0, i;
    if(strlen(pass)<7)return 0;
    for(i=0; pass[i]; i++){
        if(isupper(pass[i])) up=1;
        else if(islower(pass[i])) lo=1;
        else if(isdigit(pass[i])) di=1;
    }
    return (up && lo && di);
}

int valid_email(const char* e) {
    regex_t reg;
    const char* pat = "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$";
    if(regcomp(&reg,pat,REG_EXTENDED)!=0)return 0;
    int r = regexec(&reg,e,0,NULL,0);
    regfree(&reg);
    return (r==0);
}

int save_user(const char* u, const char* p, const char* m) {
    FILE *f = fopen("users.txt","a");
    if(!f)return -1;
    fprintf(f,"%s,%s,%s\n",u,p,m);
    fclose(f);
    return 0;
}

int main(){
    main_menu();
    return 0;
}
