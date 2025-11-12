/**
 * @file 6.ino
 * @brief All states
 *
 */

#include <AberLED.h>

// invalid state to which we initialise the state variable
#define S_INVALID -1 
// the starting state
#define S_START 0 
// the playing state
#define S_PLAYING 1
// a life has just been lost
#define S_LIFELOST 2
// the player has lost their last life
#define S_END 3

// the state variable - starts out invalid
int state = S_INVALID;
// the time the current state was entered
unsigned long stateStartTime;

// always change state by calling this function, never
// set the state variable directly
void gotoState(int s) {
    Serial.print("Going to state "); // handy for debugging!
    Serial.println(s);
    state = s;
    stateStartTime=millis();
}   

// get the time the system has been in the current state
unsigned long getStateTime(){
    return millis()-stateStartTime;
}


/**************************************************************************
 * 
 * The player model and its code
 * 
 **************************************************************************/
/*

int playerX;   // player X position
int playerLives; // number of lives remaining

// initialise the player model
void initPlayer(){
    playerX = 4;
    playerLives = 3;
}

// removes a life from the player,
// returns true if the player is now dead
bool removePlayerLife(){
    playerLives--;
    return playerLives == 0;
}

// player movement routines, limiting the motion to the screen
void movePlayerLeft() {
    if(playerX>0)
        playerX--;
}

void movePlayerRight() {
    if(playerX<7)
        playerX++;
}
*/


// draw the player (must be between clear()/swap())
void renderPlayer() {
    AberLED.set(playerX,7,GREEN);
}

// render lives in the LiveLost state as 3 lights
void renderLives(){
    AberLED.set(2,4,GREEN); // left dot always green
    if(playerLives>1) // middle dot green if lives>1
        AberLED.set(3,4,GREEN);
    else
        AberLED.set(3,4,RED);
    if(playerLives>2) // right dot green if lives>2
        AberLED.set(4,4,GREEN);
    else
        AberLED.set(4,4,RED);
}

/**************************************************************************
 * 
 * The wall block model and its code
 * 
 **************************************************************************/

// wall blocks are in an 8x8 array.
// 0 means no block present,
// 1 means 1 hit left, 2 means 2 hits left.
// less than zero means an unbreakable block.
int blocks[8][8];


// scrolling timer
#define SCROLLINTERVAL 1000 // put this line with the rest of the wall model
unsigned long lastScrollTime=0;

// create a new block at x,y
void createNewBlock(int x,int y,bool unbreakable){
    if(unbreakable)
        blocks[x][y]=-1;
    else
        blocks[x][y]=2;
}

// scroll all blocks downwards
void scrollAllBlocks() {
    for(int y=6;y>=0;y--){ // loop 6,5,4,3,2,1,0
        for(int x=0;x<8;x++){
            blocks[x][y+1] = blocks[x][y];
        }
    }
}

// damage the blocks around a block which has just been destroyed
void damageSurroundingBlocks(int bx,int by){
    // for all blocks surrounding bx,by:
    for(int x=bx-1;x<=bx+1;x++){ // loop from bx-1 to bx+1
        for(int y=by-1;y<=by+1;y++){ // loop from by-1 to by+1
            // if the block is on screen
            if(x>=0 && x<8 && y>=0 && y<8){
                // if the block is not empty and a random
                // number from 0,1 or 2 is not 0
                // (i.e. 2 out of 3 chance)
                if(blocks[x][y]!=0 && random(3)!=0)
                    // damage the block
                    blocks[x][y]--; 
            }
        }
    }
}


// hit any block at x,y with a bullet - returns true 
// if a hit occurred
bool checkBlocksForBullet(int x,int y) {  // x,y are bullet coords
    if(blocks[x][y]!=0){ // only do something if a block is there

        // decrement the hit count
        blocks[x][y]--; 
        
        // damage surrounding blocks if this block was destroyed
        if(blocks[x][y]==0)
            damageSurroundingBlocks(x,y);
        
        // and return true, which tells the calling function
        // something happened
        return true;
    }

    // otherwise nothing happened.

    return false;
}

// return true if the player has been hit
bool hasPlayerBeenHit() {

    // return whether the block at the player's
    // position is not empty - i.e. not 0

    return blocks[playerX][7]!=0;
}

// create a new row of blocks at the top of the screen - will
// overwrite anything there
void createNewTopWallBlocks() {

    // for each square at the top

    for(int x=0;x<8;x++) {

        // make the block --- but 1 in 10 times
        // make it unbreakable

        if(random(10)==0)
            createNewBlock(x,0,true);
        else
            createNewBlock(x,0,false);
    }
}   

// draw the blocks
void renderBlocks() {

    // AberLED.clear() must have been called before

    for(int x=0;x<8;x++){ // for all locations
        for(int y=0;y<8;y++) {
            switch(blocks[x][y]) { // look at the block type here
                case 0:  // don't draw anything, the location is empty
                    break;
                case 1:  // case 1 falls through...
                case 2:  // ...into case 2, so both will draw as red
                    AberLED.set(x,y,RED);
                    break;
                default: // anything else must be yellow
                    AberLED.set(x,y,YELLOW);
            }
        }
    }
    // AberLED.swap() must be called later
}

// initialise the blocks - there will no blocks on the screen
void initBlocks() {
    for(int x=0;x<8;x++){ // for all locations
        for(int y=0;y<8;y++) {
            blocks[x][y]=0;
        }
    }
}

/**************************************************************************
 * 
 * The bullet model and its code
 * 
 **************************************************************************/

// change this to change the number of bullets
#define MAXBULLETS 4

// position of the bullets if they are active
int bulletX[MAXBULLETS];
int bulletY[MAXBULLETS];

// bullet update interval in milliseconds
#define BULLETINTERVAL 100
// bullet timer variable
unsigned long lastBulletUpdateTime=0;



// which bullets are active (in flight)
bool isBulletActive[MAXBULLETS];

// make all bullets inactive
void initBullets() {
    for(int i=0;i<MAXBULLETS;i++) {
        isBulletActive[i]=false;
    }
}

// try to fire a bullet, will do nothing if MAXBULLETS bullets
// are already in flight
void fireBullet() {
    // find a spare (inactive) bullet
    for(int i=0;i<MAXBULLETS;i++) {
        if(!isBulletActive[i]) { // if bullet i is not active
            isBulletActive[i] = true; // make it active
            bulletX[i] = playerX; // set to player position
            bulletY[i] = 7; // on the bottom row
            return; // return early from function
        }
    }
    // if we got this far we were unable to fire. Not an
    // error, it just happens sometimes that we've fired
    // too many.
}

// draw the bullets
void renderBullets() {
    for(int i=0;i<MAXBULLETS;i++) {
        if(isBulletActive[i])
            AberLED.set(bulletX[i],bulletY[i],GREEN);
    }
}

// move all bullets and check them for collisions with blocks,
// deactivating them if there was a collision
void updateBullets() {
    for(int i=0;i<MAXBULLETS;i++) {
        if(isBulletActive[i]) { // only update active bullets
            // first check the wall blocks for the bullet, and
            // deactivate the bullet if it hit one.
            if(checkBlocksForBullet(bulletX[i],bulletY[i]))
                isBulletActive[i]=false;
            // move the bullet up one square
            bulletY[i]--;
            // and deactivate if it's off screen
            if(bulletY[i]<0)
                isBulletActive[i]=false;
        }
    }
}

/**************************************************************************
 * 
 * The main loop code
 * 
 **************************************************************************/

/*
 * 
 * The setup function to initialise everything
 * 
 */

void setup(){
    AberLED.begin();
    Serial.begin(9600);
    gotoState(S_START);  // start in the Start state
    initPlayer();
    initBlocks();
    initBullets();
}

/*
 * The three main loop functions : handleInput, updateModel and render. Each
 * has a state machine switch inside it, doing different things for each state.
 */

void handleInput(){
    switch(state){
    case S_START:
        // on FIRE, restart the game by reinitialising the model
        // and going into the playing state.
        if(AberLED.getButtonDown(FIRE)){
            initPlayer();
            initBlocks();
            initBullets();
            gotoState(S_PLAYING);
        }   
        break;
    case S_PLAYING:
        // handle move/fire buttons
        if(AberLED.getButtonDown(LEFT))
            movePlayerLeft();
        if(AberLED.getButtonDown(RIGHT))
            movePlayerRight();
        if(AberLED.getButtonDown(FIRE))
            fireBullet();
        break;
    case S_LIFELOST:
        break;
    case S_END:
        // fire button returns to start state
        if(AberLED.getButtonDown(FIRE))
            gotoState(S_START);
        break;
    default:
        Serial.println("Bad state in handleInput");
        break;
    }
}

// if the player has been hit, deduct a life and go to either
// the End or LifeLost state.
void checkPlayerHit(){
    if(hasPlayerBeenHit()){
        if(removePlayerLife()) // returns true if out of lives
            gotoState(S_END);
        else
            gotoState(S_LIFELOST);
    }
}

void updateModel(){
    switch(state){
    case S_START:
        break;
    case S_PLAYING:
        // move all the bullets every BULLETINTERVAL milliseconds
        if(millis()-lastBulletUpdateTime > BULLETINTERVAL) {
            lastBulletUpdateTime = millis();
            updateBullets();
        }
        // scroll the blocks every SCROLLINTERVAL milliseconds
        if(millis() - lastScrollTime > SCROLLINTERVAL) {
            lastScrollTime = millis();
            scrollAllBlocks();
            createNewTopWallBlocks();
        }
        // check the player hasn't collided
        checkPlayerHit();
        break;
    case S_LIFELOST:
        // if we're in this state for 2 seconds,
        // go back to Playing, clearing the screen
        // of blocks and bullets first.
        if(getStateTime()>2000){
            initBlocks();
            initBullets();
            gotoState(S_PLAYING);
        }
        break;
    case S_END:
        break;
    default:
        Serial.println("Bad state in update!");
        break;
    }
}

// draw a box of a given colour
void renderBox(int colour) {
    for(int i=0;i<8;i++){
        AberLED.set(0,i,colour); // left edge
        AberLED.set(7,i,colour); // right edge
        AberLED.set(i,0,colour); // top edge
        AberLED.set(i,7,colour); // bottom edge
    }
}

void render(){
    switch(state){
    case S_START:
        // just draw a green box
        renderBox(GREEN);
        break;
    case S_PLAYING:
        // draw the game 
        renderBlocks();
        renderPlayer();
        renderBullets();
        break;
    case S_LIFELOST:
        // draw a yellow box and remaining lives
        renderBox(YELLOW);
        renderLives();
        break;
    case S_END:
        // draw a red box
        renderBox(RED);
        break;
    default:
        Serial.println("Bad state in render!");
        break;
    }
}

// and we don't change anything below this point

void loop(){
    handleInput();
    updateModel();
    AberLED.clear();
    render();
    AberLED.swap();
}
