#include "rps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

rpsp_result_t det_result(rpsp_move_t move1, rpsp_move_t move2) {
    if (move1 == MOVE_NONE || move2 == MOVE_NONE) {
        return RESULT_DRAW;
    }
    if (move1 == move2) {
        return RESULT_DRAW;
    }
    if ((move1 == MOVE_ROCK && move2 == MOVE_SCISSORS) ||
        (move1 == MOVE_PAPER && move2 == MOVE_ROCK) ||
        (move1 == MOVE_SCISSORS && move2 == MOVE_PAPER)) {
        return RESULT_WIN;
    }
    return RESULT_LOSE;
}

rpsp_move_t get_move_user(){
    int input;
    printf("Enter your move (0: None, 1: Rock, 2: Paper, 3: Scissors): ");
    scanf("%d", &input);
    if (input < 0 || input > 3) {
        printf("Invalid input. Defaulting to None.\n");
        return MOVE_NONE;
    }else if (input == 0) {
        printf("You chose None.\n");
        return MOVE_NONE;
    } else if (input == 1) {
        printf("You chose Rock.\n");
        return MOVE_ROCK;
    } else if (input == 2) {
        printf("You chose Paper.\n");
        return MOVE_PAPER;
    } else if (input == 3) {
        printf("You chose Scissors.\n");
        return MOVE_SCISSORS;
    }
    return MOVE_NONE; // Default case

}

char get_tautn() {
    char taunt[256];
    printf("Enter your taunt: ");
    scanf(" %255[^\n]", taunt);
    return taunt; // Return the first character of the taunt for simplicity
}