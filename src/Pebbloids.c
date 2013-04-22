#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
//#include <stdlib.h>

#define MY_UUID { 0xEC, 0xCE, 0xA1, 0x37, 0x99, 0xBF, 0x4E, 0x48, 0x8D, 0x94, 0xEA, 0xA4, 0xBC, 0xCC, 0x9D, 0x6A }
PBL_APP_INFO(MY_UUID,
             "Pebbloids", "Adam Goldsmith",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window, gameOver;
Layer layer, scoreInfo, gameOverLayer;
int score=0, level=0, asteroidNum=0;
AppTimerHandle timer_handle;
BmpContainer shipBmp;
const int maxSpeed=5;
long seed;

typedef struct{
    int dir;
    int size;
    GPoint speed;
    GPoint pos;
} object;
const GPathInfo SHIP_SHAPE={3, (GPoint []){{10, 10}, {-10, 10}, {0, -10}}};
GPath ship_path;
object ship={.dir=0, .speed={0,0}, .pos={62,74}, .size=19};
object bullet;
object asteroids[30];

int random(int max){
    seed=(((seed*214013L+2531011L) >> 16) & 32767);
    return ((seed%max)+1);
}

void levelUp(){
    level++;
    asteroidNum=level+1;
    for(int i=0;i<level+1;i++){
        asteroids[i].speed.x=random(maxSpeed*1.5)-maxSpeed;
        asteroids[i].speed.y=random(maxSpeed*1.5)-maxSpeed;
        asteroids[i].pos.x=random(144);
        asteroids[i].pos.y=random(168);
        asteroids[i].size=random(2)*5;
    }
    layer_mark_dirty(&scoreInfo);
}

object setObject(int a, int b, GPoint c, GPoint d){
    object obj={a,b,c,d};
    return obj;
}

void reset(){
    memset(asteroids, '\0', sizeof(asteroids));
    ship=setObject(0,0,(GPoint){0,0},(GPoint){62,74});
    level=0;
    levelUp();
    score=0;
    bullet=setObject(0,0,(GPoint){0,0},(GPoint){0,0});
    window_stack_pop(true);
}

object fire(){
    object obj;
    if(ship.dir>=315||ship.dir<90){
        obj.pos.y=ship.pos.y-ship.size/2;
        obj.speed.y=-maxSpeed;
    }
    else if(ship.dir>=135&&ship.dir<=225){
        obj.pos.y=ship.pos.y+ship.size/2;
        obj.speed.y=maxSpeed;
    }
    else{
        obj.pos.y=ship.pos.y;
        obj.speed.y=0;
    }
    if(ship.dir>=45&&ship.dir<=135){
        obj.pos.x=ship.pos.x+ship.size/2;
        obj.speed.x=maxSpeed;
    }
    else if(ship.dir>=225&&ship.dir<=315){
        obj.pos.x=ship.pos.x-ship.size/2;
        obj.speed.x=-maxSpeed;
    }
    else{
        obj.pos.x=ship.pos.x;
        obj.speed.x=0;
    }
    obj.size=3;
    return obj;
}

void up_click_handler(ClickRecognizerRef recognizer, Window *window) {
    ship.dir=(ship.dir+45)%360;
    layer_mark_dirty(&layer);
}

void down_click_handler(ClickRecognizerRef recognizer, Window *window) {
    ship.dir=(ship.dir-45)%360;
    ship.dir=ship.dir<0?360+ship.dir:ship.dir;
    layer_mark_dirty(&layer);
}

void select_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
    if((ship.dir>=315||ship.dir<90)&&ship.speed.y>-maxSpeed) ship.speed.y--;
    else if(ship.dir>=135&&ship.dir<=225&&ship.speed.y<maxSpeed) ship.speed.y++;
    if(ship.dir>=45&&ship.dir<=135&&ship.speed.x<maxSpeed) ship.speed.x++;
    else if(ship.dir>=225&&ship.dir<=315&&ship.speed.x>-maxSpeed) ship.speed.x--;
    layer_mark_dirty(&layer);
}

void select_click_handler(ClickRecognizerRef recognizer, Window *window){
    bullet=fire();
}

void select_click_handler_gameOver(ClickRecognizerRef recognizer, Window *window){
    reset();
}

void config_provider(ClickConfig **config, Window *window) {
    (void)window;
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_click_handler;
    config[BUTTON_ID_SELECT]->click.repeat_interval_ms=50;
    config[BUTTON_ID_SELECT]->long_click.handler=(ClickHandler) select_long_click_handler;
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_click_handler;
    config[BUTTON_ID_UP]->click.repeat_interval_ms=50;
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_click_handler;
    config[BUTTON_ID_DOWN]->click.repeat_interval_ms=50;
}

void config_provider_gameOver(ClickConfig **config, Window *window) {
    (void)window;
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_click_handler_gameOver;
}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie){
    if (cookie==1){
        layer_mark_dirty(&layer);
        timer_handle = app_timer_send_event(ctx, 50, 1);
    }
}

void updateObject(GContext* ctx, object *obj){
    (*obj).pos.x=(*obj).pos.x+(*obj).speed.x;
    (*obj).pos.y=(*obj).pos.y+(*obj).speed.y;
    if((*obj).pos.x+21<0) (*obj).pos.x=144;
    else if((*obj).pos.x>144) (*obj).pos.x=0;
    if((*obj).pos.y+21<0) (*obj).pos.y=168;
    else if((*obj).pos.y>168) (*obj).pos.y=0;
}

int detectCollision(object *obj, int lastObj){
    for(int i=lastObj>-1?lastObj:0;i<asteroidNum;i++){
        if(((*obj).pos.x-(*obj).size/2<asteroids[i].pos.x+asteroids[i].size/2)&&((*obj).pos.x+(*obj).size/2>asteroids[i].pos.x-asteroids[i].size/2)&&((*obj).pos.y-(*obj).size/2<asteroids[i].pos.y+asteroids[i].size/2)&&((*obj).pos.y+(*obj).size/2>asteroids[i].pos.y-asteroids[i].size/2)) return i;
    }
    return -1;
}

void update_layer_callback(Layer *me, GContext* ctx){
    gpath_rotate_to(&ship_path, (TRIG_MAX_ANGLE/360)*ship.dir);
    updateObject(ctx, &ship);
    updateObject(ctx, &bullet);
    for(int i=0;i<asteroidNum;i++){
        updateObject(ctx, &asteroids[i]);
        graphics_fill_circle(ctx,asteroids[i].pos,asteroids[i].size);
    }
    if(bullet.size>0){
        int asteroidHit=detectCollision(&bullet, -1);
        if(asteroidHit>-1){
            score++;
            asteroidNum--;
            layer_mark_dirty(&scoreInfo);
            asteroids[asteroidHit]=setObject(0,0,(GPoint){0,0},(GPoint){0,0});
        }
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_draw_circle(ctx,bullet.pos, bullet.size);
    }
    if(asteroidNum==0) levelUp();
    if(detectCollision(&ship, -1)>-1) window_stack_push(&gameOver, true);
    for(int i=0;i<asteroidNum;i++){
        detectCollision(&asteroids[i], i);
    }
    gpath_move_to(&ship_path, ship.pos);
    gpath_draw_filled(ctx, &ship_path);
}

void update_scoreInfo_callback(Layer *me, GContext* ctx){
    char str[24]="Score: 0000    Level: 00";
    int num=score;
    for(int i=10;i>6&&num>0;i--){
        str[i]=(num%10)+48;
        num/=10;
    }
    num=level;
    for(int i=23;i>21&&num>0;i--){
        str[i]=(num%10)+48;
        num/=10;
    }
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_text_draw(ctx,
                       str,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(3, 1, 144, 19),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentLeft,
                       NULL);
}

void update_gameOver_callback(Layer *me, GContext* ctx){
    graphics_context_set_text_color(ctx, GColorBlack);
    char str[24]="Score: 0000    Level: 00";
    int num=score;
    for(int i=10;i>6&&num>0;i--){
        str[i]=(num%10)+48;
        num/=10;
    }
    num=level;
    for(int i=23;i>21&&num>0;i--){
        str[i]=(num%10)+48;
        num/=10;
    }
    graphics_text_draw(ctx,
                       str,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(3, 1, 144, 19),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentLeft,
                       NULL);
    graphics_text_draw(ctx,
                       "GAME OVER.\n\nPress select to restart.",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(3, 19, 144, 160),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentLeft,
                       NULL);
}

void handle_init(AppContextRef ctx) {
    window_init(&window, "Pebbloids");
    window_init(&gameOver, "Game Over");
    window_stack_push(&window, true);
    window_set_fullscreen(&window, true);
    layer_init(&layer, window.layer.frame);
    layer_init(&scoreInfo, layer.frame);
    layer_init(&gameOverLayer, gameOver.layer.frame);
    layer.update_proc=update_layer_callback;
    scoreInfo.update_proc=update_scoreInfo_callback;
    gameOverLayer.update_proc=update_gameOver_callback;
    layer_add_child(&window.layer, &layer);
    layer_add_child(&layer, &scoreInfo);
    layer_add_child(&gameOver.layer, &gameOverLayer);
    resource_init_current_app(&APP_RESOURCES);
    layer_mark_dirty(&layer);
    layer_mark_dirty(&scoreInfo);
    window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
    window_set_click_config_provider(&gameOver, (ClickConfigProvider) config_provider_gameOver);
    timer_handle = app_timer_send_event(ctx, 50, 1);
    gpath_init(&ship_path, &SHIP_SHAPE);
    levelUp();
    PblTm time;
    get_time(&time);
    seed=time.tm_hour*3600+time.tm_min*60+time.tm_sec;
}


void pbl_main(void *params) {
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .timer_handler = &handle_timer
    };
    app_event_loop(params, &handlers);
}
