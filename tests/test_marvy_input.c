#include "marvy_input.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static uint32_t send(marvy_input_t *input,marvy_session_t *s,
                     unsigned button,bool pressed,uint64_t at)
{
    const marvy_input_event_t event={(uint8_t)button,pressed,at};
    return marvy_input_handle(input,s,&event);
}
static void start(marvy_input_t *input,marvy_session_t *s,unsigned level)
{
    marvy_input_init(input);marvy_session_init(s,123,17,0);
    marvy_progress_t progress;marvy_progress_init(&progress);
    progress.unlocked_count=(uint8_t)(level+1);marvy_game_apply_progress(&s->game,&progress);
    send(input,s,MARVY_BUTTON_UP,true,0);send(input,s,MARVY_BUTTON_UP,false,0);
    s->game.level_index=(uint8_t)level;
    assert(send(input,s,MARVY_BUTTON_OK,true,0)==MARVY_EVENT_START);
    send(input,s,MARVY_BUTTON_OK,false,0);
    for(unsigned i=0;i<MARVY_SHEEP_COUNT;i++)s->game.sheep[i].hidden_ms=UINT32_MAX;
}

static void test_welcome_brief_and_charge_ownership(void)
{
    marvy_input_t input;marvy_session_t s;
    marvy_input_init(&input);marvy_session_init(&s,1,0,0);
    assert(send(&input,&s,99,true,0)==0 && input.held_button==-1);
    assert(send(&input,&s,MARVY_BUTTON_UP,false,0)==0);
    send(&input,&s,MARVY_BUTTON_UP,true,10);
    assert(s.game.phase==MARVY_PHASE_BRIEF && input.held_button==MARVY_BUTTON_UP);
    send(&input,&s,MARVY_BUTTON_OK,true,11);send(&input,&s,MARVY_BUTTON_DOWN,false,12);
    assert(s.game.phase==MARVY_PHASE_BRIEF && input.held_button==MARVY_BUTTON_UP);
    send(&input,&s,MARVY_BUTTON_UP,false,13);
    assert(send(&input,&s,MARVY_BUTTON_OK,true,20)==MARVY_EVENT_START);
    assert(send(&input,&s,MARVY_BUTTON_OK,false,21)==0 && s.game.throws==0);
    send(&input,&s,MARVY_BUTTON_DOWN,true,100);
    send(&input,&s,MARVY_BUTTON_OK,true,110);send(&input,&s,MARVY_BUTTON_OK,false,111);
    assert(s.game.phase==MARVY_PHASE_PLAYING && input.held_button==MARVY_BUTTON_DOWN);
    assert(send(&input,&s,MARVY_BUTTON_DOWN,false,800)&MARVY_EVENT_THROW);
    assert(s.game.throws==1 && s.game.ring_target_x==120);
    assert(send(&input,&s,MARVY_BUTTON_DOWN,false,801)==0);
}

static void test_timeout_owned_release_never_throws_again(void)
{
    marvy_input_t input;marvy_session_t s;start(&input,&s,3);
    send(&input,&s,MARVY_BUTTON_UP,true,100);
    assert(marvy_session_advance(&s,6000)==MARVY_EVENT_THROW);
    assert(s.button_held && input.held_button==MARVY_BUTTON_UP && s.game.throws==1);
    send(&input,&s,MARVY_BUTTON_DOWN,true,6001);
    send(&input,&s,MARVY_BUTTON_DOWN,false,6002);
    marvy_session_advance(&s,6800);
    assert(s.game.ring_state==MARVY_RING_READY);
    assert(send(&input,&s,MARVY_BUTTON_UP,false,6801)==0);
    assert(!s.button_held && input.held_button==-1 && s.game.throws==1);
    send(&input,&s,MARVY_BUTTON_UP,true,6802);
    assert(send(&input,&s,MARVY_BUTTON_UP,false,6902)==MARVY_EVENT_THROW);
    assert(s.game.throws==2);
}

static void test_overflow_stale_cancel_do_not_reset_either_clock(void)
{
    marvy_input_t input;marvy_session_t s;start(&input,&s,3);
    send(&input,&s,MARVY_BUTTON_DOWN,true,100);marvy_session_advance(&s,800);
    marvy_input_cancel(&input,&s);
    assert(input.held_button==-1 && !s.button_held && s.game.ring_state==MARVY_RING_READY);
    assert(s.game.stage_elapsed_ms==800 && s.game.turn_ms==800 && s.game.throws==0);
    assert(send(&input,&s,MARVY_BUTTON_DOWN,false,801)==0);
    send(&input,&s,MARVY_BUTTON_DOWN,true,900);marvy_session_advance(&s,1000);
    assert(send(&input,&s,MARVY_BUTTON_DOWN,false,999)==0);
    assert(input.held_button==-1 && !s.button_held && s.game.ring_state==MARVY_RING_READY);
    assert(s.game.stage_elapsed_ms==1000 && s.game.turn_ms==1000);
    assert(send(&input,&s,MARVY_BUTTON_UP,true,999)==0 && input.held_button==-1);
    assert(send(&input,&s,99,false,1001)==0);
    assert(marvy_session_advance(&s,6000)==MARVY_EVENT_THROW);
}

static void test_pause_exit_and_wake_releases_are_inert(void)
{
    marvy_input_t input;marvy_session_t s;start(&input,&s,0);
    send(&input,&s,MARVY_BUTTON_OK,true,1000);
    assert(s.game.phase==MARVY_PHASE_PAUSED);
    send(&input,&s,MARVY_BUTTON_DOWN,true,1001);send(&input,&s,MARVY_BUTTON_UP,false,1002);
    assert(s.game.pause_selection==MARVY_PAUSE_CONTINUE);
    send(&input,&s,MARVY_BUTTON_OK,false,1003);
    send(&input,&s,MARVY_BUTTON_DOWN,true,1100);send(&input,&s,MARVY_BUTTON_DOWN,false,1101);
    assert(s.game.pause_selection==MARVY_PAUSE_EXIT);
    assert(marvy_session_advance(&s,46101)==MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input,&s);
    assert(send(&input,&s,MARVY_BUTTON_OK,true,50000)==MARVY_SESSION_EVENT_WAKE);
    assert(send(&input,&s,MARVY_BUTTON_OK,false,50001)==0);
    assert(s.game.phase==MARVY_PHASE_PAUSED && s.game.stage_elapsed_ms==1000);
    send(&input,&s,MARVY_BUTTON_OK,true,50002);
    assert(s.game.phase==MARVY_PHASE_INTRO);
    send(&input,&s,MARVY_BUTTON_OK,true,50003);send(&input,&s,MARVY_BUTTON_OK,false,50004);
    assert(s.game.phase==MARVY_PHASE_INTRO && s.game.throws==0);
    send(&input,&s,MARVY_BUTTON_UP,true,50005);send(&input,&s,MARVY_BUTTON_UP,false,50006);
    assert(s.game.phase==MARVY_PHASE_BRIEF);
}

static void test_press_at_finish_cannot_select_or_restart_results(void)
{
    marvy_input_t input;marvy_session_t s;start(&input,&s,0);
    s.game.stage_elapsed_ms=44999;s.game.captures=4;s.game.throws=4;s.game.score=4;
    assert(send(&input,&s,MARVY_BUTTON_OK,true,1)&MARVY_EVENT_FINISH);
    assert(s.game.phase==MARVY_PHASE_RESULTS && s.game.result_selection==MARVY_RESULT_NEXT);
    send(&input,&s,MARVY_BUTTON_DOWN,true,2);send(&input,&s,MARVY_BUTTON_DOWN,false,3);
    assert(s.game.result_selection==MARVY_RESULT_NEXT);
    send(&input,&s,MARVY_BUTTON_OK,false,4);assert(s.game.phase==MARVY_PHASE_RESULTS);
    send(&input,&s,MARVY_BUTTON_OK,true,5);send(&input,&s,MARVY_BUTTON_OK,false,6);
    assert(s.game.phase==MARVY_PHASE_BRIEF && s.game.level_index==1 && s.game.throws==0);
    send(&input,&s,MARVY_BUTTON_OK,true,7);send(&input,&s,MARVY_BUTTON_OK,false,8);
    assert(s.game.phase==MARVY_PHASE_PLAYING && s.game.throws==0);
}

int main(void)
{
    test_welcome_brief_and_charge_ownership();
    test_timeout_owned_release_never_throws_again();
    test_overflow_stale_cancel_do_not_reset_either_clock();
    test_pause_exit_and_wake_releases_are_inert();
    test_press_at_finish_cannot_select_or_restart_results();
    puts("Marvy campaign input ownership, cancellation and transition tests: PASS");
    return 0;
}
