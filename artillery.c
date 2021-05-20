#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159
#define WIDTH 640
#define HEIGHT 480

#define P1_UP ALLEGRO_KEY_W
#define P1_DOWN ALLEGRO_KEY_S
#define P1_LEFT ALLEGRO_KEY_A
#define P1_RIGHT ALLEGRO_KEY_D
#define P1_FIRE ALLEGRO_KEY_LSHIFT

#define P2_UP ALLEGRO_KEY_UP
#define P2_DOWN ALLEGRO_KEY_DOWN
#define P2_LEFT ALLEGRO_KEY_LEFT
#define P2_RIGHT ALLEGRO_KEY_RIGHT
#define P2_FIRE ALLEGRO_KEY_RSHIFT

ALLEGRO_TIMER *gTimer;
ALLEGRO_EVENT_QUEUE *gQueue;
ALLEGRO_DISPLAY *gDisp;
ALLEGRO_FONT *gFont;

ALLEGRO_BITMAP *gPlayerSheet;
ALLEGRO_BITMAP *gMap;

struct Player
{
	int x, y, dir, pdir, cooldown, charge;
	float a, am;
	bool moving;
	bool firing;
	bool dead;
};

struct Missile
{
	int x, y, xv, yv;
};

struct Explosion
{
	int x, y, r;
	bool expanding;
};

struct Player gP1, gP2;
struct Missile *gMissiles[20];
struct Explosion *gExplosions[20];

void ensure(bool cond, const char *desc, const char *op)
{
	if(cond) return;
	printf("Failed to %s %s!\n", op, desc);
	exit(1);
}

#define ensureInit(C, D) ensure(C, D, "initialize")
#define ensureLoad(B, F) ensure(B = al_load_bitmap(F), F, "load")

void initAllegro()
{
	ensureInit(al_init(), "allegro");
	ensureInit(al_init_image_addon(), "image addon");
	ensureInit(al_install_keyboard(), "keyboard");

	ensureInit(gTimer = al_create_timer(1.0 / 30.0), "timer");
	ensureInit(gQueue = al_create_event_queue(), "event queue");
	ensureInit(gDisp = al_create_display(WIDTH, HEIGHT), "display");
	ensureInit(gFont = al_create_builtin_font(), "font");

	al_register_event_source(gQueue, al_get_keyboard_event_source());
	al_register_event_source(gQueue, al_get_display_event_source(gDisp));
	al_register_event_source(gQueue, al_get_timer_event_source(gTimer));

	ensureLoad(gPlayerSheet, "player.png");
	gMap = al_create_bitmap(WIDTH, HEIGHT);

	time_t t;
	srand((unsigned)(time(&t)));
}

void endAllegro()
{
	al_destroy_bitmap(gPlayerSheet);
	al_destroy_bitmap(gMap);

	al_destroy_font(gFont);
	al_destroy_display(gDisp);
	al_destroy_event_queue(gQueue);
	al_destroy_timer(gTimer);
}

unsigned char fAlpha(ALLEGRO_COLOR c);

void initPlayer(struct Player *p, int x, int dir)
{
	p->x = x;
	p->y = HEIGHT-64;
	p->a = 0;
	p->am = 0;
	p->dir = gP1.pdir = dir;
	p->cooldown = 0;
	p->firing = false;
	p->dead = false;
	p->moving = false;
	p->charge = 0;
	while(fAlpha(al_get_pixel(gMap, p->x+16, p->y+32)))
		p->y--;
}

void initGame()
{
	al_lock_bitmap(gMap, ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_WRITEONLY);
	al_set_target_bitmap(gMap);
	al_clear_to_color(al_map_rgba(0,0,0,0));

	int x, y, h = 50 + rand() % 200;
	float w = 50 + rand() % 100;
	for(x=0; x<WIDTH; x++)
		for(y=HEIGHT-sinf(x/w)*10-h; y<HEIGHT; y++)
			al_put_pixel(x, y, al_map_rgb(0xe0, 0xe0, 0xa0));

	al_unlock_bitmap(gMap);
	al_set_target_backbuffer(gDisp);

	al_lock_bitmap(gMap, ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_READONLY);

	initPlayer(&gP1, 10, 0);
	initPlayer(&gP2, WIDTH-50, ALLEGRO_FLIP_HORIZONTAL);

	al_unlock_bitmap(gMap);

	int i;
	for(i=0; i<20; i++)
	{
		gMissiles[i] = 0;
		gExplosions[i] = 0;
	}
}

void endGame()
{
	int i, j;
	for(i=0; gMissiles[i]!=0; i++);
	for(j=0; j<i; j++)
		free(gMissiles[i]);
	for(i=0; gExplosions[i]!=0; i++);
	for(j=0; j<i; j++)
		free(gExplosions[i]);
}

unsigned char fAlpha(ALLEGRO_COLOR c)
{
	unsigned char r, g, b, a;
	al_unmap_rgba(c, &r, &g, &b, &a);
	return a;
}

void movePlayer(struct Player *p, int xm, int ym)
{
	p->x+=xm;
	p->y+=ym;

	if(p->x < -2) p->x = -2;
	if(p->x+32-2 > WIDTH) p->x = WIDTH-32+2;
	if(p->y+32 > HEIGHT) p->y = HEIGHT-32;

	al_lock_bitmap(gMap, ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_READONLY);

	int x, y, i, j;
	for(x = p->x+5; x < p->x+32-5; x++)
	{
		for(i=0; i<15 && fAlpha(al_get_pixel(gMap, x, p->y+30)); i++)
			p->y--;
		if(i==15) p->y+=15;
		if(x==p->x+5) j=i;
	}

	if(i==15||j==15)
	{
		p->y-=ym;
		p->x-=xm;
	}

	al_unlock_bitmap(gMap);
}

void playerFire(struct Player *p)
{
	if(p->cooldown!=0) return;
	p->cooldown = 20;

	int i;
	for(i=0; gMissiles[i]!=0; i++);
	gMissiles[i] = malloc(sizeof(struct Missile));
	gMissiles[i]->x = p->x + cosf(p->a)*16;
	gMissiles[i]->y = p->y + sinf(p->a)*16;
	gMissiles[i]->xv = cosf(p->a-PI/2)*p->charge;
	gMissiles[i]->yv = sinf(p->a-PI/2)*p->charge;

	p->charge = 0;
}

void explodeMissile(struct Missile *m)
{
	int i, j;
	for(i=0; gExplosions[i]!=0; i++);
	gExplosions[i] = malloc(sizeof(struct Explosion));
	gExplosions[i]->x = m->x;
	gExplosions[i]->y = m->y;
	gExplosions[i]->r = 3;
	gExplosions[i]->expanding = true;

	for(i=0; gMissiles[i]!=m; i++);
	for(j=0; gMissiles[j+1]!=0; j++);
	free(m);
	gMissiles[i] = 0;
	gMissiles[i] = gMissiles[j];
	gMissiles[j] = 0;
}

void updateMissiles()
{
	al_lock_bitmap(gMap, ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_READONLY);

	int i;
	struct Missile *m;
	for(i=0; gMissiles[i]!=0; i++)
	{
		m = gMissiles[i];

		m->x += m->xv;
		m->y += m->yv;

		m->yv++;

		if(m->y >= HEIGHT)
		{
			explodeMissile(m);
			i--;
			continue;
		}
		if(m->x >= 0 && m->x < WIDTH && m->y >= 0)
			if(fAlpha(al_get_pixel(gMap, m->x, m->y)))
			{
				explodeMissile(m);
				i--;
				continue;
			}
	}

	al_unlock_bitmap(gMap);
}

bool playerAt(struct Player p, int x, int y)
{
	if(x > p.x+6 && x < p.x+26 && y > p.y+6 && y < p.y+30)
		return true;
	else
		return false;
}

void updateExplosions()
{
	al_lock_bitmap(gMap, ALLEGRO_PIXEL_FORMAT_ANY,
			ALLEGRO_LOCK_READWRITE);
	al_set_target_bitmap(gMap);

	int i, j, x, y, r;
	float a;
	struct Explosion *e;
	for(i=0; gExplosions[i]!=0; i++)
	{
		e = gExplosions[i];

		if(e->expanding)
		{
			for(a=0; a<=PI*2; a+=0.01)
				for(r=0; r<=e->r; r++)
				{
					x = e->x+cosf(a)*r;
					y = e->y+sinf(a)*r;

					if(playerAt(gP1, x, y))
						gP1.dead = true;
					if(playerAt(gP2, x, y))
						gP2.dead = true;

					if(x<0||y<0||x>=WIDTH||y>=HEIGHT)
						continue;

					if(fAlpha(al_get_pixel(gMap, x, y)))
						al_put_pixel(x, y,
							al_map_rgba(0,0,0,0));
				}

			e->r+=5;
			if(e->r > 50)
				e->expanding = false;
		}
		else
		{
			e->r--;
			if(e->r < 25)
			{
				for(j=0; gExplosions[j+1]!=0; j++);
				free(e);
				gExplosions[i] = 0;
				gExplosions[i] = gExplosions[j];
				gExplosions[j] = 0;
				i--;
				continue;
			}
		}
	}

	al_unlock_bitmap(gMap);
	al_set_target_backbuffer(gDisp);
}

void drawPlayer(struct Player p, ALLEGRO_COLOR tint)
{
	if(p.dead)
		al_draw_tinted_scaled_rotated_bitmap_region(gPlayerSheet,
				96, 32, 32, 32, tint, 0, 0, p.x, p.y, 1, 1,
				0, 0);
	else if(p.moving)
		al_draw_tinted_scaled_rotated_bitmap_region(gPlayerSheet,
				32+32*((p.x/2)%2), 0, 32, 32, tint, 0, 0,
				p.x, p.y, 1, 1, 0, p.dir);
	else
		al_draw_tinted_scaled_rotated_bitmap_region(gPlayerSheet,
				0, 0, 32, 32, tint, 0, 0, p.x, p.y,
				1, 1, 0, p.dir);

	if(!p.dead)
		al_draw_tinted_scaled_rotated_bitmap_region(gPlayerSheet,
				96, 0, 32, 32, tint, 16, 16,
				p.x+16, p.y+16, 1, 1, p.a, p.dir);
}

void drawPlayerStats(struct Player p, const char *name, int x, int y)
{
	char stats1[16];
	char stats2[32];
	char stats3[32];
	bool draw3 = true;
	ALLEGRO_COLOR c = al_map_rgb(0xff, 0xff, 0xff);
	const char *doa[2] = {"alive", "dead"};
	sprintf(stats1, "status:%s", doa[p.dead]);
	sprintf(stats2, "x:%d y:%d   a:%d", p.x, HEIGHT-p.y,
			(int)(p.a/PI*180));
	if(p.charge!=0)
		sprintf(stats3, "charge:%d", p.charge);
	else if(p.cooldown!=0)
		sprintf(stats3, "cooldown:%d", p.cooldown);
	else
		draw3 = false;

	al_draw_text(gFont, c, x, y, 0, name);
	al_draw_text(gFont, c, x, y+8, 0, stats1);
	al_draw_text(gFont, c, x, y+16, 0, stats2);
	if(draw3 && !p.dead)
		al_draw_text(gFont, c, x, y+24, 0, stats3);
}

void drawMissiles()
{
	int i;
	struct Missile *m;
	float a;
	for(i=0; gMissiles[i]!=0; i++)
	{
		m = gMissiles[i];
		al_draw_tinted_scaled_rotated_bitmap_region(gPlayerSheet,
				0, 32, 32, 32, al_map_rgb(0xff, 0xff, 0xff),
				16, 16, m->x, m->y, 1, 1,
				atan2(m->yv, m->xv)+PI/2, 0);
	}
}

void drawExplosions()
{
	int i;
	float s;
	struct Explosion *e;
	for(i=0; gExplosions[i]!=0; i++)
	{
		e = gExplosions[i];
		s = (e->r+4) / 16.0;
		if(e->expanding)
			al_draw_tinted_scaled_rotated_bitmap_region(
					gPlayerSheet, 32, 32, 32, 32,
					al_map_rgb(0xff, 0xff, 0xff), 16, 16,
					e->x, e->y, s, s, 0, 0);
		else
			al_draw_tinted_scaled_rotated_bitmap_region(
					gPlayerSheet, 64, 32, 32, 32,
					al_map_rgba(0xff, 0xff, 0xff, 0x88),
					16, 16, e->x, e->y, s, s, 0, 0);
	}
}

void draw()
{
	al_clear_to_color(al_map_rgb(0x60, 0x90, 0xa0));

	al_draw_bitmap(gMap, 0, 0, 0);

	drawPlayer(gP1, al_map_rgb(0xff, 0xe0, 0xa0));
	drawPlayer(gP2, al_map_rgb(0xa0, 0xe0, 0xff));

	drawMissiles();
	drawExplosions();

	drawPlayerStats(gP1, "Player 1", 0, 0);
	drawPlayerStats(gP2, "Player 2", WIDTH/2, 0);

	al_flip_display();
}

void updatePlayer(struct Player *p)
{
	movePlayer(p, 0, 2);

	if(p->dead) return;

	if(p->moving)
		p->dir ? movePlayer(p, -1, 0) : movePlayer(p, 1, 0);

	if(p->dir)
		p->a -= p->am;
	else
		p->a += p->am;

	if(p->a < -PI/4) p->a = -PI/4;
	if(p->a > PI/4) p->a = PI/4;

	if(p->pdir != p->dir) p->a *= -1;
	p->pdir = p->dir;

	if(p->cooldown > 0) p->cooldown--;

	if(p->firing)
	{
		if(p->cooldown == 0)
			p->charge++;
		else
			p->firing = false;
	}
	else if(p->charge != 0)
		playerFire(p);
}

void controlPlayer(struct Player *p, ALLEGRO_EVENT event,
		int up, int down, int left, int right, int fire)
{
	if(p->dead) return;

	int k;
	switch(event.type)
	{
		case ALLEGRO_EVENT_KEY_DOWN:
			k = event.keyboard.keycode;
			if(k==up)
				p->am = -0.01;
			if(k==down)
				p->am = 0.01;
			if(k==left)
			{
				p->dir = ALLEGRO_FLIP_HORIZONTAL;
				p->moving = true;
			}
			if(k==right)
			{
				p->dir = 0;
				p->moving = true;
			}
			if(k==fire)
				p->firing = true;
			break;
		case ALLEGRO_EVENT_KEY_UP:
			k = event.keyboard.keycode;
			if(k==up||k==down)
				p->am = 0;
			if(k==left||k==right)
				p->moving = false;
			if(k==fire)
				p->firing = false;
			break;
	}
}

#define controlP1(E) controlPlayer(&gP1, E,\
		P1_UP, P1_DOWN, P1_LEFT, P1_RIGHT, P1_FIRE)
#define controlP2(E) controlPlayer(&gP2, E,\
		P2_UP, P2_DOWN, P2_LEFT, P2_RIGHT, P2_FIRE)

void haltPlayer(struct Player *p)
{
	p->moving = false;
	p->am = 0;
	p->firing = false;
}

int main()
{
	initAllegro();
	initGame();

	bool redraw = true, quit = false;
	float aMod = 0;
	ALLEGRO_EVENT event;

	al_start_timer(gTimer);
	while(!quit)
	{
		al_wait_for_event(gQueue, &event);
		switch(event.type)
		{
			case ALLEGRO_EVENT_TIMER:
				updatePlayer(&gP1);
				updatePlayer(&gP2);
				updateMissiles();
				updateExplosions();
				redraw = true;
				break;
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				quit = true;
				break;
			case ALLEGRO_EVENT_KEY_DOWN:
				controlP1(event);
				controlP2(event);
				break;
			case ALLEGRO_EVENT_KEY_UP:
				switch(event.keyboard.keycode)
				{
					case ALLEGRO_KEY_ESCAPE:
						quit = true;
						break;
					case ALLEGRO_KEY_R:
						endGame();
						initGame();
						redraw = true;
						break;
				}
				controlP1(event);
				controlP2(event);
				break;
			case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
				haltPlayer(&gP1);
				haltPlayer(&gP2);
				break;
		}

		if(redraw && al_is_event_queue_empty(gQueue))
		{
			draw();
			redraw = false;
		}
	}

	endGame();
	endAllegro();
	return 0;
}
