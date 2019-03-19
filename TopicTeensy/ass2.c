#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <sprite.h>
#include <lcd.h>
#include "lcd_model.h"

// Game Constants
#define DELAY 10 /* milliseconds */
#define HERO_HEIGHT 3 
#define HERO_WIDTH 3
#define ZOMBIE_HEIGHT 3
#define ZOMBIE_WIDTH 3
#define TREASURE_HEIGHT 2 
#define TREASURE_WIDTH 3
#define FOOD_HEIGHT 2
#define FOOD_WIDTH 3
#define PLATFORM_HEIGHT 2
#define PLATFORM_WIDTH 10
#define MAX_PLATFORMS (160)
#define ROW_SPACING 6
#define MAX_FOOD (5)
#define MAX_ZOMBIES (5)
#define NUMBER_OF_ROWS (LCD_Y / (PLATFORM_HEIGHT + (HERO_HEIGHT + 2)))
#define NUMBER_OF_COLUMNS (LCD_X / (PLATFORM_WIDTH + 2))
#define FREQ (8000000.0)
#define PRESCALE (64.0)


// Game States
bool game_over = false;
bool platform_collision = false;


// Basic Variables & Inventory
char score_buffer[20];
char food_buffer[20];
char lives_buffer[20];
char zombie_buffer[20];
char time_buffer[20];
int score = 0;
int lives = 10;
int current_food_total = 0;
int food_in_inv = 5;


static uint8_t food_bitmap[] = {
		0b11011111,
		0b01111111,
		0b10111111,
	};

static uint8_t safe_platform_bitmap[] = {
    	0b11111111,
		0b11111111,
		0b11111111,
		0b11111111,
	};

    static uint8_t unsafe_platform_bitmap[] = {
    	0b11111111,
		0b11000000,
		0b10101010,
		0b10000000,
	};


// Game Variables
Sprite hero;
Sprite treasure;
Sprite zombie[5];
Sprite food[MAX_FOOD];
double gravity = 0.35;
double dx = 0;
double dy = 0;


// Platform Variables
Sprite platforms[12];
Sprite unsafe_platforms[5];
Sprite starting_block;

int plat_x[12];
int plat_y[12];
int unsafe_plat_x[5];
int unsafe_plat_y[5];

// Clear All 
void setup_controls(){
    CLEAR_BIT(DDRD, 1); // JoyStick Up
    CLEAR_BIT(DDRB, 7); // JoyStick Down
	CLEAR_BIT(DDRB, 1); // JoyStick Left
    CLEAR_BIT(DDRD, 0); // JoyStick Right
    CLEAR_BIT(DDRB, 0); // JoyStick Centre
}

// Timer
void setup_timers (void) {
	// Initialise Timer 1
	TCCR1A = 0;
	TCCR1B = 3;
	TIMSK1 = 1;
	
	// Initialise Timer 0
	TCCR0A = 0;
	TCCR0B = 4;
	TIMSK0 = 1;
	
	//Turn on interrupts
	sei();
} // end setup_timers


// Game Timer
volatile uint32_t overflow_counter = 0;
ISR(TIMER1_OVF_vect) {
	{
		overflow_counter++;
	}
} // end ISR Timer 1

double current_time (void) {

	return (overflow_counter * 65536.0 + TCNT1) * PRESCALE / FREQ;

} 

void paused_game() {

    // char score_buffer[10];
    // char food_buffer[10];
    // char lives_buffer[10];
    // char zombie_buffer[10];
    // char time_buffer[10];

    // Display Score, Lives, Number of Zombies on Screen, Number of Food in Inventory
    char *pause = "PAUSED";
    snprintf(score_buffer, sizeof(score_buffer), "Score: %d", score);
    snprintf(food_buffer, sizeof(food_buffer), "Food Left: %d", food_in_inv - current_food_total);
    // snprintf(zombie_buffer, sizeof(zombie_buffer), "Zombies: %d", MAX_ZOMBIES - 5);
    // snprintf(time_buffer, 11, "Time: %02f", current_time());
    snprintf(lives_buffer, sizeof(lives), "Lives: %d", lives);

    draw_string(24, 0, pause, FG_COLOUR);
	draw_string(1, 10, score_buffer, FG_COLOUR);
    draw_string(1, 20, food_buffer, FG_COLOUR);
    // draw_string(1, 30, zombie_buffer, FG_COLOUR);
    draw_string(1, 30, lives_buffer, FG_COLOUR);


    show_screen();

}



bool sprites_collide(Sprite sprite_one, Sprite sprite_two) 
{
    // Sprite One
    int top1 = round(sprite_one.y);
    int right1 = round(sprite_one.x) + sprite_one.width; 
    int bottom1 = (sprite_one.y) + sprite_one.height;
    int left1 = round(sprite_one.x); 
    
    // Sprite Two
    int top2 = round(sprite_two.y);
    int right2 = round(sprite_two.x) + sprite_two.width; 
    int bottom2 = (sprite_two.y) + sprite_two.height;
    int left2 = round(sprite_two.x); 

    // Box Collision
    if (top1 > bottom2)
    {
        return false;
    }
    else if (top2 > bottom1)
    {
        return false;
    }
    else if (right1 < left2)
    {
        return false;
    }
    else if (right2 < left1)
    {
        return false;
    }
    else { 
        return true;
    }
}


// Collision for When Hero contacts any Platform
sprite_id sprites_collide_any(Sprite s, Sprite sprite_platforms[], int number) 
{
    
    sprite_id result = NULL;

    for (int i = 0; i < number; i++)
    {
        if(sprites_collide(s, sprite_platforms[i]))
        {
            result = &sprite_platforms[i];
            // score++;
            break;
        }
    }

    return result;
}





void platform_setup() {
    // Create Bit Map

    // New Random Seed
    srand(TCNT1);

    // Starting Block
    sprite_init(&starting_block, 0, 8, 10, 2, safe_platform_bitmap);
	sprite_draw(&starting_block);

	// Platform Sprite
	// X & Y Platform Coordinates

    // For Testing 
	// int plat_x[12] = {0,2,3,4,5,6,1,2,3,4,5,6};
	// int plat_y[12] = {2,1,3,4,5,2,3,4,5,1,3,4};
    

    for (int i = 0; i < 12; i++) {
        int x_pos = (rand() % 6) + 1;
        *(plat_x + i) = x_pos;
    }

    for (int i = 0; i < 12; i++) {
        int y_pos = (rand() % 6) + 1;
        *(plat_y + i) = y_pos;
    }

    for (int i = 0; i < 5; i++) {
        int unsafex_pos = (rand() % 6) + 1;
        *(unsafe_plat_x + i) = unsafex_pos;
    }

    for (int i = 0; i < 5; i++) {
        int unsafey_pos = (rand() % 6) + 1;
        *(unsafe_plat_y + i) = unsafey_pos;
    }

}

void draw_platforms () {

    // UnSafe Platforms
    for (int i = 0; i < 5; i++) {
		sprite_init(&unsafe_platforms[i], (unsafe_plat_x[i] * 12), (unsafe_plat_y[i] * 8), 10, 2, unsafe_platform_bitmap);
		sprite_draw(&unsafe_platforms[i]);
	}
    // Safe Platforms
	for (int i = 0; i < 12; i++) {
		sprite_init(&platforms[i], (plat_x[i] * 12), (plat_y[i] * 8), 10, 2, safe_platform_bitmap);
		sprite_draw(&platforms[i]);
	}
}


// Hero Setup
void hero_setup() {
    // Create Bit Map
    static uint8_t bitmap[] = {
		0b11111111,
		0b10111111,
		0b11111111,
	};
	
	sprite_init(&hero, 3, 1, HERO_WIDTH, HERO_HEIGHT, bitmap);
	sprite_draw(&hero);
}

// Reset Hero to Start Block on Event or Collision
void reposition_playerevent() {
    
    // Reset Hero Position
    hero_setup();

}

// Hero Movement
void hero_movement() {
      
    //Jump
    if (dy < 1) {

        if ( BIT_IS_SET(PIND, 1) ) hero.y = hero.y - 0.75;

        
    }

    if (dy > 0) {

        // No Movement When Falling
        if ( BIT_IS_SET(PINB, 1) ) hero.x = 0, dx = 0;
	    if ( BIT_IS_SET(PIND, 0) ) hero.x = 0, dx = 0;

    } else {
        // Left & Right Movement
        if ( BIT_IS_SET(PINB, 1) ) hero.x = hero.x - 0.4, dy = 0;
        if ( BIT_IS_SET(PIND, 0) ) hero.x = hero.x + 0.4, dy = 0;
    }
    
	if ( dx || dy ) {
			hero.x += dx;
			hero.y += dy;
	}

	// Stop Player DX / DY on Wall Collision
	if (hero.x > (LCD_X - 5) || hero.x <= 0) {

		reposition_playerevent();
        lives--;
        
	}
	if (hero.y >= (LCD_Y - 3) || hero.y <= 0) {

		reposition_playerevent();
        lives--;
	}

    if (dy > 0) {

        platform_collision = false;

    }
}


void zombie_setup() {

        static uint8_t zombie_bitmap[] = {
		0b11011111,
		0b11111111,
		0b10111111,
	};
    
    
    int zombie_fall_positions[5] = {12,24,36,48,72};

    for (int z = 0; z < MAX_ZOMBIES; z++) {

        sprite_init(&zombie[z], zombie_fall_positions[z], -15, ZOMBIE_WIDTH, ZOMBIE_HEIGHT, zombie_bitmap);
	    sprite_draw(&zombie[z]);

    }
}



void zombie_movement() {

    for (int i = 0; i < MAX_ZOMBIES; i++) {

        zombie[i].dy = 0;
        zombie[i].dx = 0;
        // zombie[i].x += zombie[i].dx;

        // int zl = (int) round(zombie[i].x);
        // int zr = zl + ZOMBIE_WIDTH - 1;
    

        // if (zl < 0 || zr >= LCD_X) {

        //     zombie[i].x -= zombie[i].dx;
        //     zombie[i].dx = -zombie[i].dx;

        // }
    }
}


// Treasure
void treasure_setup() {

        static uint8_t treasure_bitmap[] = {
		0b10111111,
		0b11111111,
		0b10111111,
	};

    sprite_init(&treasure, (LCD_X / 2), (LCD_Y - 3), TREASURE_WIDTH, TREASURE_HEIGHT, treasure_bitmap);
    treasure.dy = 0;
    treasure.dx = 0.3;
	sprite_draw(&treasure);

}


void treasure_movement() {

    static bool treasure_paused = false;

    // Pause Treasure
    if (BIT_IS_SET(PINF, 5)) {

        while (BIT_IS_SET(PINF, 5)) {

        treasure_paused = !treasure_paused;
        
        }
    }

    if (!treasure_paused) {
        treasure.x += treasure.dx;
        treasure.dy = 0;
        int tl = (int) round(treasure.x);
        int tr = tl + TREASURE_WIDTH - 1;


        if (tl < 0 || tr >= LCD_X) {
            treasure.dy = 0;
            treasure.x -= treasure.dx;
            treasure.dx = -treasure.dx;

        }

    }
    
}
  


// Screen Before GamePlay
void welcome_screen() {
	
    draw_string( 20, 10, "MITCH FOX", FG_COLOUR);
	draw_string( 22, 20, "N9968296", FG_COLOUR);
    draw_string( 2, (LCD_Y / 2 + 12), "THE HEROIC DONUT", FG_COLOUR);
}

// Screen Before GamePlay
void game_over_screen() {
	
    draw_string( 20, 10, "GAME OVER", FG_COLOUR);
	draw_string( 22, 20, "N9968296", FG_COLOUR);
    draw_string( 2, (LCD_Y / 2 + 12), "THE HEROIC DONUT", FG_COLOUR);

}

void setup( void ) {
    // Define Initial Contrast & Speed
	set_clock_speed(CPU_8MHz);
	lcd_init(LCD_DEFAULT_CONTRAST);

	// Welcome Screen & Introduction of Stud No.
    while (!BIT_IS_SET(PINF, 6)){
        welcome_screen();
        show_screen(); 
    }

    clear_screen();
	
    // Reset the Joystick Bits
	setup_controls();

	// Setup Screen for GamePlay
    hero_setup();
    treasure_setup();
    platform_setup();
    zombie_setup();

    // // New Random Seed
    // srand(TCNT1);

	
    // Show Screen *Keep 
    show_screen();

}



void process(void) {
	
    clear_screen();
    
    if (lives > 0) {

        // // PAUSED GAME
        if (BIT_IS_SET(PINB, 0)) {
            clear_screen();

            while (BIT_IS_SET(PINB, 0)) {

            paused_game();

            }

            show_screen();

        }

        // HERO COLLISION
        // Collision With Starting Block
        if (sprites_collide(hero, starting_block))
        {
            hero.dy = 0;
            score++;
        }
        // Collision With Safe Block
        else if (sprites_collide_any(hero, platforms, 12))
        {
            hero.dy = 0;
            platform_collision = true;
            score++;
            
        }

        else if (sprites_collide_any(hero, zombie, MAX_ZOMBIES))
        {
            reposition_playerevent();
            lives--;
        }
        // Collision With Unsafe Block
        else if (sprites_collide_any(hero, unsafe_platforms, 5))
        {
            reposition_playerevent();
            lives--;
        }
        // Collision With Treasure
        else if (sprites_collide(hero, treasure))
        {
            reposition_playerevent();
            lives = lives + 2;
            // Treasure Dissapear for Rest of Game
            treasure.x = 1000;
            treasure.y = 100;

        }
        else
        // No Collision & Apply Gravity
        { 
            // Apply Gravity
            hero.y = hero.y + gravity;
        }  



        // ZOMBIE COLLISION
        // Collision With Safe Block
        
        for (int i = 0; i < MAX_ZOMBIES; i++) {

            if (sprites_collide_any(zombie[i], platforms, 12))
            {
                zombie[i].dy = 0;
            }
            
        }

        for (int i = 0; i < MAX_ZOMBIES; i++) {

            if (!sprites_collide_any(zombie[i], platforms, 12)) {

                zombie[i].y = zombie[i].y + gravity;
            }
        }

        

        // Drop Food

        if (BIT_IS_SET(PINB, 7)) {

            if (current_food_total < MAX_FOOD) {

            sprite_id undropped_food = &food[current_food_total];
            int x = hero.x;
            int y = hero.y;
            sprite_init(undropped_food, x, y, FOOD_WIDTH, FOOD_HEIGHT, food_bitmap);
            current_food_total = current_food_total + 1;

        } 
        
        } else {

            // No More Food

            }
        
    

    


        


        
        // Game Functions
        platform_setup();
        
        // Movements 
        hero_movement();
        treasure_movement();
        zombie_movement();

        // Draw
        sprite_draw(&hero);
        sprite_draw(&treasure);
        draw_platforms();
        

        for (int i = 0; i < MAX_ZOMBIES; i++) {
            sprite_draw(&zombie[i]);
        }
        for (int i = 0; i < current_food_total; i++) {
            sprite_draw(&food[i]);
        }

    // Show Screen 
    show_screen();

    } else {

        game_over = true;

        clear_screen();
        game_over_screen();
        show_screen();

    }
    
}


int main(void) {
	setup();

	for ( ;; ) {

        if (!game_over) {

            process();

        }
        else if (game_over == true) {

            if (BIT_IS_SET(PINF, 6)) {
                
                game_over = false;
                clear_screen();

                process();

            }
        }
	}
}