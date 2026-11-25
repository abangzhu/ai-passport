#include "marvy_session.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void clear_lane(marvy_session_t *s)
{
    for (unsigned i=0;i<MARVY_SHEEP_COUNT;i++)s->game.sheep[i].hidden_ms=UINT32_MAX;
}
static void start(marvy_session_t *s,unsigned level,uint64_t at)
{
    marvy_session_init(s,123,17,at);
    marvy_progress_t p;marvy_progress_init(&p);p.unlocked_count=(uint8_t)(level+1);
    marvy_game_apply_progress(&s->game,&p);
    assert(marvy_session_press_button(s,MARVY_BUTTON_UP,at)==0);
    assert(s->game.phase==MARVY_PHASE_BRIEF);
    marvy_session_release(s,at);
    s->game.level_index=(uint8_t)level;
    assert(marvy_session_press_button(s,MARVY_BUTTON_OK,at)==MARVY_EVENT_START);
    assert(marvy_session_release(s,at)==0);
    clear_lane(s);
}

static void test_brief_requires_confirm_and_consumes_all_releases(void)
{
    marvy_session_t s;marvy_session_init(&s,1,9,0);
    assert(marvy_session_advance(&s,10000)==0 && s.game.phase==MARVY_PHASE_INTRO);
    assert(marvy_session_press_button(&s,MARVY_BUTTON_OK,10001)==0);
    assert(s.game.phase==MARVY_PHASE_BRIEF && s.game.stage_elapsed_ms==0);
    marvy_session_release(&s,10002);
    marvy_session_press_button(&s,MARVY_BUTTON_DOWN,10003);
    marvy_session_release(&s,10004);
    assert(s.game.phase==MARVY_PHASE_BRIEF && s.game.level_index==0);
    marvy_session_advance(&s,30000);assert(s.game.stage_elapsed_ms==0);
    assert(marvy_session_press_button(&s,MARVY_BUTTON_OK,30001)==MARVY_EVENT_START);
    marvy_session_release(&s,30002);
    assert(s.game.throws==0 && s.game.ring_state==MARVY_RING_READY);
}

static void test_timestamped_charge_duplicate_and_stale_edges(void)
{
    marvy_session_t s;start(&s,0,100);
    marvy_session_press(&s,200);
    assert(marvy_session_press(&s,500)==0 && s.game.ring_ms==300);
    marvy_session_advance(&s,900);
    assert(marvy_session_release(&s,899)==0 && s.button_held);
    assert(marvy_session_press(&s,899)==0);
    assert(marvy_session_advance(&s,899)==0 && s.now_ms==900);
    assert(marvy_session_release(&s,900)==MARVY_EVENT_THROW);
    assert(s.game.throws==1 && s.game.ring_target_x==120);
    assert(marvy_session_release(&s,901)==0);
    marvy_session_cancel_input(&s);
    assert(s.game.ring_state==MARVY_RING_FLYING);
    assert(marvy_session_advance(&s,1500)==MARVY_EVENT_MISS);
}

static void test_no_timeout_first_level_and_unattended_end(void)
{
    marvy_session_t s;start(&s,0,0);
    marvy_session_press(&s,100);
    assert(marvy_session_advance(&s,44999)==0 && !s.dimmed);
    assert(s.game.throws==0 && s.game.ring_state==MARVY_RING_CHARGING);
    uint32_t events=marvy_session_advance(&s,45000);
    assert(events&MARVY_EVENT_FINISH);assert(events&MARVY_EVENT_LEVEL_FAIL);
    assert(!(events&MARVY_SESSION_EVENT_DIM));
    assert(s.game.phase==MARVY_PHASE_RESULTS && s.last_input_ms==45000);
    assert(marvy_session_release(&s,45001)==0);
    assert(marvy_session_advance(&s,90000)==0);
    assert(marvy_session_advance(&s,90001)==MARVY_SESSION_EVENT_DIM);
}

static void test_pause_deadline_and_flight_freeze(void)
{
    marvy_session_t s;start(&s,3,0);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,5999);
    assert(s.game.phase==MARVY_PHASE_PAUSED && s.game.turn_ms==5999);
    assert(s.game.stage_elapsed_ms==5999);
    marvy_session_release(&s,6000);
    marvy_game_t frozen=s.game;
    assert(marvy_session_advance(&s,10000)==0 && memcmp(&frozen,&s.game,sizeof(frozen))==0);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,10001);
    assert(s.game.phase==MARVY_PHASE_PLAYING && s.game.throws==0);
    assert(marvy_session_release(&s,10001)==0);
    assert(marvy_session_advance(&s,10002)==MARVY_EVENT_THROW);
    assert(s.game.stage_elapsed_ms==6000 && s.game.throws==1);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,10102);
    marvy_session_release(&s,10103);assert(s.game.ring_ms==100);
    marvy_session_advance(&s,20000);assert(s.game.ring_ms==100);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,20001);
    marvy_session_release(&s,20001);
    assert(marvy_session_advance(&s,20501)==MARVY_EVENT_MISS);
    start(&s,3,0);
    assert(marvy_session_press_button(&s,MARVY_BUTTON_OK,6000)==MARVY_EVENT_THROW);
    assert(s.game.phase==MARVY_PHASE_PAUSED && s.game.ring_state==MARVY_RING_FLYING);
}

static void test_wake_does_not_start_confirm_or_throw(void)
{
    marvy_session_t s;start(&s,0,0);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,1000);marvy_session_release(&s,1001);
    marvy_session_press_button(&s,MARVY_BUTTON_DOWN,1100);marvy_session_release(&s,1101);
    assert(marvy_session_advance(&s,46101)==MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s,MARVY_BUTTON_OK,50000)==MARVY_SESSION_EVENT_WAKE);
    assert(marvy_session_release(&s,50001)==0 && s.game.phase==MARVY_PHASE_PAUSED);
    assert(s.game.pause_selection==MARVY_PAUSE_EXIT);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,50002);
    marvy_session_release(&s,50003);
    assert(s.game.phase==MARVY_PHASE_INTRO && s.game.score==0 && s.game.best_score==17);
    assert(marvy_session_advance(&s,95003)==MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s,MARVY_BUTTON_UP,95004)==MARVY_SESSION_EVENT_WAKE);
    marvy_session_release(&s,95005);assert(s.game.phase==MARVY_PHASE_INTRO);
    marvy_session_press_button(&s,MARVY_BUTTON_UP,95006);
    marvy_session_release(&s,95007);assert(s.game.phase==MARVY_PHASE_BRIEF);
    assert(marvy_session_advance(&s,140007)==MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s,MARVY_BUTTON_OK,140008)==MARVY_SESSION_EVENT_WAKE);
    marvy_session_release(&s,140009);assert(s.game.phase==MARVY_PHASE_BRIEF);
}

static void test_finish_press_is_consumed_before_result_menu(void)
{
    marvy_session_t s;start(&s,0,0);
    s.game.stage_elapsed_ms=44999;s.game.captures=4;s.game.throws=4;s.game.score=4;
    uint32_t events=marvy_session_press_button(&s,MARVY_BUTTON_DOWN,1);
    assert(events&MARVY_EVENT_FINISH);
    assert(s.game.phase==MARVY_PHASE_RESULTS && s.game.result_selection==MARVY_RESULT_NEXT);
    assert(marvy_session_release(&s,2)==0 && s.game.phase==MARVY_PHASE_RESULTS);
    marvy_session_press_button(&s,MARVY_BUTTON_DOWN,3);marvy_session_release(&s,4);
    assert(s.game.result_selection==MARVY_RESULT_RETRY);
    marvy_session_press_button(&s,MARVY_BUTTON_OK,5);marvy_session_release(&s,6);
    assert(s.game.phase==MARVY_PHASE_BRIEF && s.game.level_index==0 && s.game.throws==0);
    marvy_session_press_button(&s,MARVY_BUTTON_DOWN,7);marvy_session_release(&s,8);
    assert(s.game.level_index==1);
    marvy_session_press_button(&s,MARVY_BUTTON_UP,9);marvy_session_release(&s,10);
    assert(s.game.level_index==0);
}

static void test_cancel_invalid_and_uint64_boundaries(void)
{
    marvy_session_t s;start(&s,3,0);
    assert(marvy_session_press_button(&s,(marvy_button_t)99,100)==0 && s.now_ms==0);
    assert(marvy_session_press_button(&s,(marvy_button_t)-1,100)==0);
    marvy_session_press(&s,100);marvy_session_advance(&s,800);
    marvy_session_cancel_input(&s);
    assert(!s.button_held && s.game.ring_state==MARVY_RING_READY && s.game.turn_ms==800);
    assert(marvy_session_release(&s,801)==0);
    assert(marvy_session_advance(&s,UINT64_MAX)&MARVY_EVENT_FINISH);
    assert(s.now_ms==UINT64_MAX && s.last_input_ms==UINT64_MAX && !s.dimmed);
    assert(marvy_session_press(&s,0)==0 && marvy_session_release(&s,0)==0);
    start(&s,0,UINT64_MAX-40000);marvy_session_advance(&s,UINT64_MAX);
    assert(s.game.phase==MARVY_PHASE_PLAYING && !s.dimmed && s.game.stage_elapsed_ms==40000);
    marvy_session_init(&s,1,0,0);
    assert(marvy_session_advance(&s,UINT64_MAX)==MARVY_SESSION_EVENT_DIM);
    assert(s.now_ms==UINT64_MAX);
}

int main(void)
{
    test_brief_requires_confirm_and_consumes_all_releases();
    test_timestamped_charge_duplicate_and_stale_edges();
    test_no_timeout_first_level_and_unattended_end();
    test_pause_deadline_and_flight_freeze();
    test_wake_does_not_start_confirm_or_throw();
    test_finish_press_is_consumed_before_result_menu();
    test_cancel_invalid_and_uint64_boundaries();
    puts("Marvy campaign timestamp, pause and menu session tests: PASS");
    return 0;
}
