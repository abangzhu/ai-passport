#include "marvy_game.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void clear_lane(marvy_game_t *g)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) g->sheep[i].hidden_ms = UINT32_MAX;
}

static void start(marvy_game_t *g, unsigned level)
{
    marvy_game_init(g, 123, 17);
    marvy_progress_t p;
    marvy_progress_init(&p); p.unlocked_count = (uint8_t)(level + 1);
    marvy_game_apply_progress(g, &p);
    assert(marvy_game_press(g) == 0 && g->phase == MARVY_PHASE_BRIEF);
    g->level_index = (uint8_t)level;
    assert(marvy_game_press(g) == MARVY_EVENT_START);
    assert(g->phase == MARVY_PHASE_PLAYING);
    clear_lane(g);
}

static uint32_t catch_one(marvy_game_t *g, bool marvy)
{
    assert(g->ring_state == MARVY_RING_READY);
    clear_lane(g);
    g->sheep[0].x = 32; g->sheep[0].speed = 0;
    g->sheep[0].marvy = marvy; g->sheep[0].hidden_ms = 0;
    g->sheep[0].announced = true;
    marvy_game_press(g);
    assert(marvy_game_release(g) == MARVY_EVENT_THROW);
    uint32_t events = marvy_game_step(g, MARVY_FLIGHT_MS);
    assert(events & MARVY_EVENT_HIT);
    marvy_game_step(g, MARVY_FEEDBACK_MS);
    assert(g->ring_state == MARVY_RING_READY);
    return events;
}

static void test_level_configuration_and_brief(void)
{
    const unsigned goals[] = {4,5,6,7,9,10};
    const unsigned speed[] = {70,70,90,90,115,115};
    const unsigned turn[] = {0,0,0,6000,6000,4500};
    for (unsigned i=0;i<MARVY_LEVEL_COUNT;i++) {
        const marvy_level_config_t *c=marvy_game_level_config(i);
        assert(c && c->duration_ms==45000 && c->target_catches==goals[i]);
        assert(c->speed_percent==speed[i] && c->turn_limit_ms==turn[i]);
        assert(c->show_target==(i==0));
        assert(c->marvy_percent==(i<4?25:30) && c->marvy_cap==(i<4?1:2));
    }
    assert(marvy_game_level_config(MARVY_LEVEL_COUNT)==NULL);
    marvy_game_t g; marvy_game_init(&g,0,9);
    assert(g.phase==MARVY_PHASE_INTRO && g.progress.unlocked_count==1);
    marvy_game_step(&g,60000); assert(g.phase==MARVY_PHASE_INTRO);
    assert(marvy_game_press(&g)==0 && g.phase==MARVY_PHASE_BRIEF);
    marvy_game_brief_move(&g,1); assert(g.level_index==0);
    marvy_game_step(&g,60000); assert(g.stage_elapsed_ms==0);
    assert(marvy_game_press(&g)==MARVY_EVENT_START);
    assert(g.phase==MARVY_PHASE_PLAYING && g.throws==0);
    assert(marvy_game_release(&g)==0);
}

static void test_unified_focus_and_confirm_actions(void)
{
    marvy_game_t g;
    marvy_game_init(&g, 123, 17);
    assert(g.intro_selection == MARVY_INTRO_START);
    marvy_game_intro_move(&g, 0);
    assert(g.intro_selection == MARVY_INTRO_START);
    marvy_game_intro_move(&g, -1);
    assert(g.intro_selection == MARVY_INTRO_CONTROLS && g.phase == MARVY_PHASE_INTRO);
    assert(marvy_game_press(&g) == 0 && g.phase == MARVY_PHASE_CONTROLS);
    marvy_game_step(&g, 60000);
    assert(g.stage_elapsed_ms == 0 && g.throws == 0);
    marvy_game_intro_move(&g, 1);
    assert(g.phase == MARVY_PHASE_CONTROLS);
    assert(marvy_game_press(&g) == 0 && g.phase == MARVY_PHASE_INTRO);
    assert(g.intro_selection == MARVY_INTRO_START);
    marvy_game_intro_move(&g, 1); marvy_game_intro_move(&g, 1);
    assert(g.intro_selection == MARVY_INTRO_START);
    assert(marvy_game_press(&g) == 0 && g.phase == MARVY_PHASE_BRIEF);
    assert(marvy_game_press(&g) == MARVY_EVENT_START);
    assert(g.play_selection == MARVY_PLAY_THROW);
    clear_lane(&g);
    marvy_game_step(&g, 1234);
    marvy_game_play_move(&g, -1);
    assert(g.play_selection == MARVY_PLAY_PAUSE && g.phase == MARVY_PHASE_PLAYING);
    assert(g.stage_elapsed_ms == 1234 && g.turn_ms == 1234 && g.throws == 0);
    assert(marvy_game_press(&g) == 0 && g.phase == MARVY_PHASE_PAUSED);
    marvy_game_pause_move(&g, -1);
    assert(g.pause_selection == MARVY_PAUSE_EXIT);
    marvy_game_pause_move(&g, 1);
    assert(g.pause_selection == MARVY_PAUSE_CONTINUE);
    assert(marvy_game_press(&g) == 0 && g.phase == MARVY_PHASE_PLAYING);
    assert(g.play_selection == MARVY_PLAY_THROW && g.turn_ms == 1234);
    marvy_game_play_move(&g, 0);
    assert(g.play_selection == MARVY_PLAY_THROW);
    assert(marvy_game_press(&g) == 0 && g.ring_state == MARVY_RING_CHARGING);
    assert(marvy_game_release(&g) == MARVY_EVENT_THROW);
    marvy_game_play_move(&g, 1);
    assert(marvy_game_press(&g) == 0 && g.phase == MARVY_PHASE_PAUSED);
    assert(g.ring_state == MARVY_RING_FLYING);
    marvy_game_pause_move(&g, 1); marvy_game_pause_confirm(&g);
    assert(g.phase == MARVY_PHASE_INTRO && g.intro_selection == MARVY_INTRO_START);
    marvy_game_play_move(&g, 1); marvy_game_pause_move(&g, 1);
    assert(g.phase == MARVY_PHASE_INTRO && g.play_selection == MARVY_PLAY_THROW);
}

static void test_first_three_levels_have_no_throw_deadline(void)
{
    for(unsigned level=0;level<3;level++) {
        marvy_game_t g; start(&g,level);
        assert(marvy_game_step(&g,20000)==0);
        assert(g.throws==0 && marvy_game_countdown(&g)==0);
        marvy_game_press(&g); marvy_game_step(&g,10000);
        assert(g.ring_state==MARVY_RING_CHARGING && g.throws==0);
        assert(g.ring_ms==MARVY_CHARGE_MS && marvy_game_countdown(&g)==0);
        assert(marvy_game_release(&g)==MARVY_EVENT_THROW);
        assert(g.ring_target_x==208 && g.throws==1);
    }
}

static void test_throw_deadlines_and_cancel_do_not_extend_turn(void)
{
    marvy_game_t g; start(&g,3);
    marvy_game_step(&g,2999); assert(marvy_game_countdown(&g)==0);
    marvy_game_step(&g,1); assert(marvy_game_countdown(&g)==3);
    marvy_game_press(&g); marvy_game_step(&g,1000);
    assert(marvy_game_countdown(&g)==2);
    marvy_game_cancel_charge(&g); assert(g.turn_ms==4000);
    marvy_game_step(&g,1000); assert(marvy_game_countdown(&g)==1);
    marvy_game_press(&g); marvy_game_step(&g,999);
    assert(g.throws==0 && g.ring_state==MARVY_RING_CHARGING);
    assert(marvy_game_step(&g,1)==MARVY_EVENT_THROW);
    assert(g.throws==1 && g.ring_target_x==120 && marvy_game_release(&g)==0);
    marvy_game_step(&g,800);
    assert(g.ring_state==MARVY_RING_READY && g.turn_ms==0);
    start(&g,5); marvy_game_step(&g,1500); assert(marvy_game_countdown(&g)==3);
    marvy_game_step(&g,3000); assert(g.throws==1 && g.ring_target_x==120);
    g.stage_elapsed_ms=40000; g.ring_state=MARVY_RING_READY; g.turn_ms=3000;
    assert(marvy_game_countdown(&g)==0); /* Overall final-five warning wins. */
}

static void test_unlimited_throws_uniform_lock_and_independent_celebration(void)
{
    marvy_game_t g; start(&g,0);
    for(unsigned i=0;i<12;i++) catch_one(&g,false);
    assert(g.throws==12 && g.captures==12 && g.score==12);
    catch_one(&g,true);
    assert(g.throws==13 && g.captures==13 && g.score==17);
    assert(g.celebration_ms==1000 && g.celebration_points==5);
    float celebration_x=g.celebration_x;
    marvy_game_press(&g); marvy_game_step(&g,100);
    assert(marvy_game_release(&g)==MARVY_EVENT_THROW);
    assert(g.celebration_ms==900 && g.celebration_x==celebration_x);
    marvy_game_step(&g,900); assert(g.celebration_ms==0);
    assert(marvy_game_feedback_ms(&g)==200);
}

static void test_combo_events_and_miss_reset(void)
{
    marvy_game_t g; start(&g,0);
    assert(!(catch_one(&g,false)&MARVY_EVENT_COMBO_3));
    catch_one(&g,false);
    assert(catch_one(&g,true)&MARVY_EVENT_COMBO_3);
    assert(g.combo_count==3 && g.combo_ms>0);
    catch_one(&g,false);
    assert(catch_one(&g,false)&MARVY_EVENT_COMBO_5);
    assert(g.hit_streak==5 && g.best_streak==5 && g.combo_count==5);
    clear_lane(&g); marvy_game_press(&g); marvy_game_release(&g);
    assert(marvy_game_step(&g,600)==MARVY_EVENT_MISS);
    assert(g.hit_streak==0 && g.best_streak==5);
}

static void test_stage_end_cancels_unreleased_and_blocks_simultaneous_auto(void)
{
    marvy_game_t g; start(&g,0); marvy_game_step(&g,44999);
    marvy_game_press(&g);
    uint32_t events=marvy_game_step(&g,1);
    assert((events&(MARVY_EVENT_FINISH|MARVY_EVENT_LEVEL_FAIL))==
           (MARVY_EVENT_FINISH|MARVY_EVENT_LEVEL_FAIL));
    assert(g.phase==MARVY_PHASE_RESULTS && g.time_expired && g.throws==0);
    assert(g.stage_elapsed_ms==45000 && marvy_game_remaining_ms(&g)==0);
    assert(marvy_game_release(&g)==0 && marvy_game_step(&g,1000)==0);
    start(&g,3); g.stage_elapsed_ms=44999; g.turn_ms=5999;
    events=marvy_game_step(&g,1);
    assert((events&MARVY_EVENT_THROW)==0 && g.phase==MARVY_PHASE_RESULTS && g.throws==0);
}

static void test_last_airborne_ring_gets_full_landing_and_result_once(void)
{
    marvy_game_t g; start(&g,0);
    g.stage_elapsed_ms=44999; g.captures=3; g.score=3; g.throws=3;
    g.sheep[0].x=32; g.sheep[0].speed=0; g.sheep[0].hidden_ms=0;
    g.sheep[0].marvy=false; g.sheep[0].announced=true;
    marvy_game_press(&g); marvy_game_release(&g);
    assert(marvy_game_step(&g,1)==0 && g.phase==MARVY_PHASE_PLAYING && g.time_expired);
    assert(marvy_game_press(&g)==0 && marvy_game_release(&g)==0);
    assert(marvy_game_step(&g,599)==MARVY_EVENT_HIT);
    assert(g.captures==4 && g.stage_elapsed_ms==45000);
    uint32_t events=marvy_game_step(&g,200);
    assert(events&MARVY_EVENT_FINISH); assert(events&MARVY_EVENT_LEVEL_CLEAR);
    assert(events&MARVY_EVENT_PROGRESS_CHANGED);
    assert(g.phase==MARVY_PHASE_RESULTS && g.passed && g.stars==1);
    assert(g.progress.unlocked_count==2 && g.progress.best_score[0]==4);
    assert(marvy_game_step(&g,60000)==0);
}

static void finish_fixture(marvy_game_t *g,unsigned captures,unsigned throws)
{
    g->captures=(uint16_t)captures; g->throws=(uint16_t)throws;
    g->score=(uint16_t)captures; g->stage_elapsed_ms=44999;
    marvy_game_step(g,1);
    assert(g->phase==MARVY_PHASE_RESULTS);
}

static void test_stars_progress_and_result_navigation(void)
{
    marvy_game_t g; start(&g,0); finish_fixture(&g,3,3); assert(!g.passed && g.stars==0);
    assert(g.result_selection==MARVY_RESULT_RETRY && !marvy_game_can_next(&g));
    marvy_game_result_move(&g,-1); assert(g.result_selection==MARVY_RESULT_WELCOME);
    marvy_game_result_confirm(&g); assert(g.phase==MARVY_PHASE_INTRO);
    start(&g,0); finish_fixture(&g,6,12); assert(g.stars==2);
    assert(g.result_selection==MARVY_RESULT_NEXT && marvy_game_can_next(&g));
    marvy_game_result_confirm(&g); assert(g.phase==MARVY_PHASE_BRIEF && g.level_index==1);
    marvy_game_brief_move(&g,-1); assert(g.level_index==0);
    marvy_game_brief_move(&g,0); assert(g.level_index==0);
    start(&g,0); finish_fixture(&g,8,14); assert(g.stars==2);
    start(&g,0); finish_fixture(&g,8,13); assert(g.stars==3);
    start(&g,5); finish_fixture(&g,14,20); assert(g.stars==3 && !marvy_game_can_next(&g));
    assert(g.progress.unlocked_count==6 && g.progress.best_stars[5]==3);
    marvy_game_result_confirm(&g); assert(g.phase==MARVY_PHASE_BRIEF && g.level_index==5);
}

static void test_pause_exit_preserves_only_completed_progress(void)
{
    marvy_game_t g; start(&g,3); catch_one(&g,true);
    marvy_game_t before=g;
    marvy_game_pause(&g); assert(g.phase==MARVY_PHASE_PAUSED);
    uint32_t clock=g.stage_elapsed_ms, celebration=g.celebration_ms;
    assert(marvy_game_step(&g,60000)==0);
    assert(g.stage_elapsed_ms==clock && g.celebration_ms==celebration);
    marvy_game_pause_select(&g,(marvy_pause_selection_t)99);
    assert(g.pause_selection==MARVY_PAUSE_CONTINUE);
    marvy_game_pause_confirm(&g); assert(g.phase==MARVY_PHASE_PLAYING && g.score==before.score);
    marvy_game_press(&g); marvy_game_step(&g,100); clock=g.turn_ms;
    marvy_game_pause(&g); assert(g.ring_state==MARVY_RING_READY && g.turn_ms==clock);
    marvy_game_pause_select(&g,MARVY_PAUSE_EXIT); marvy_game_pause_confirm(&g);
    assert(g.phase==MARVY_PHASE_INTRO && g.score==0 && g.best_score==17);
    assert(g.progress.best_score[3]==0 && g.celebration_ms==0);
    assert(marvy_game_release(&g)==0);
}

static void test_campaign_records_do_not_replace_the_legacy_ten_ring_best(void)
{
    marvy_game_t g; start(&g,0);
    g.captures=20;g.throws=25;g.score=100;g.stage_elapsed_ms=44999;
    uint32_t events=marvy_game_step(&g,1);
    assert(events&MARVY_EVENT_PROGRESS_CHANGED);
    assert((events&MARVY_EVENT_NEW_BEST)==0);
    assert(g.best_score==17 && g.progress.best_score[0]==100);
}

static void test_spawn_bags_speed_and_cap(void)
{
    for(unsigned level=0;level<6;level++) {
        marvy_game_t g; start(&g,level);
        unsigned count=0, samples=level<4?4:10;
        for(unsigned i=0;i<samples;i++) {
            g.sheep[0].x=500; g.sheep[0].hidden_ms=0;
            marvy_game_step(&g,1); count+=g.sheep[0].marvy;
            float speed=g.sheep[0].speed; if(speed<0)speed=-speed;
            float ratio=marvy_game_level_config(level)->speed_percent/100.0f;
            assert(speed>=(g.sheep[0].marvy?48:30)*ratio-0.001f);
            assert(speed<=(g.sheep[0].marvy?56:42)*ratio+0.001f);
        }
        assert(count==(level<4?1:3));
    }
    marvy_game_t g; start(&g,0);
    g.sheep[0].hidden_ms=0;g.sheep[0].x=120;g.sheep[0].speed=0;g.sheep[0].marvy=true;
    g.spawn_bag_size=1;g.spawn_bag_next=0;g.spawn_bag[0]=1;
    g.sheep[1].x=500;g.sheep[1].hidden_ms=0;g.sheep[1].marvy=false;
    marvy_game_step(&g,1); assert(g.sheep[1].hidden_ms>0 && g.spawn_bag_next==0);
    g.sheep[0].hidden_ms=100000;marvy_game_step(&g,200);
    assert(g.sheep[1].hidden_ms==0 && g.sheep[1].marvy);
}

static void test_mapping_saturation_and_progress_validation(void)
{
    assert(marvy_game_charge_target_x(0)==32 && marvy_game_charge_target_x(700)==120);
    assert(marvy_game_charge_target_x(1400)==208 && marvy_game_charge_target_x(UINT32_MAX)==208);
    marvy_progress_t a,b;marvy_progress_init(&a);marvy_progress_init(&b);
    assert(marvy_progress_valid(&a) && !marvy_progress_valid(NULL));
    b.unlocked_count=6;b.best_score[5]=25;b.best_stars[5]=3;
    assert(marvy_progress_merge(&a,&b));assert(!marvy_progress_merge(&a,&b));
    b.best_stars[5]=4;assert(!marvy_progress_merge(&a,&b));
    b.best_stars[5]=0;b.unlocked_count=0;assert(!marvy_progress_valid(&b));
    b.unlocked_count=7;assert(!marvy_progress_valid(&b));
    marvy_game_t g;start(&g,0);g.score=UINT16_MAX;g.throws=UINT16_MAX;
    g.captures=UINT16_MAX;g.hit_streak=UINT16_MAX;g.best_streak=UINT16_MAX;
    catch_one(&g,true);
    assert(g.score==UINT16_MAX && g.captures==UINT16_MAX && g.throws==UINT16_MAX);
    assert(g.hit_streak==UINT16_MAX);
    marvy_game_apply_progress(&g,&b);assert(g.progress.unlocked_count==1);
    g.phase=MARVY_PHASE_INTRO;g.phase_ms=UINT32_MAX-1;marvy_game_step(&g,2);
    assert(g.phase_ms==UINT32_MAX);
}

static void test_initial_brand_guarantee_and_closest_visible_hit(void)
{
    marvy_game_t g,copy;marvy_game_init(&g,123,0);marvy_game_init(&copy,123,0);
    assert(memcmp(&g,&copy,sizeof(g))==0);
    unsigned brands=0;
    for(unsigned i=0;i<MARVY_SHEEP_COUNT;i++)brands+=g.sheep[i].marvy;
    assert(brands==1);
    start(&g,0);
    for(unsigned i=0;i<3;i++) {
        g.sheep[i].hidden_ms=0;g.sheep[i].speed=0;g.sheep[i].announced=true;
    }
    g.sheep[0].x=48;g.sheep[0].marvy=false;
    g.sheep[1].x=47;g.sheep[1].marvy=true;
    g.sheep[2].x=17;g.sheep[2].marvy=false;
    marvy_game_press(&g);marvy_game_release(&g);
    assert(marvy_game_step(&g,600)==(MARVY_EVENT_HIT|MARVY_EVENT_MARVY_HIT));
    assert(g.last_hit_sheep==1 && g.captures==1 && g.score==5);
    marvy_game_step(&g,200);clear_lane(&g);
    g.sheep[0].hidden_ms=0;g.sheep[0].x=48;g.sheep[0].speed=0;g.sheep[0].marvy=false;
    marvy_game_press(&g);marvy_game_release(&g);
    assert(marvy_game_step(&g,600)==MARVY_EVENT_HIT); /* Inclusive +/-16px. */
    marvy_game_step(&g,200);clear_lane(&g);
    g.sheep[0].hidden_ms=0;g.sheep[0].x=49;g.sheep[0].speed=0;g.sheep[0].marvy=false;
    marvy_game_press(&g);marvy_game_release(&g);
    assert(marvy_game_step(&g,600)==MARVY_EVENT_MISS);
}

int main(void)
{
    test_level_configuration_and_brief();
    test_unified_focus_and_confirm_actions();
    test_first_three_levels_have_no_throw_deadline();
    test_throw_deadlines_and_cancel_do_not_extend_turn();
    test_unlimited_throws_uniform_lock_and_independent_celebration();
    test_combo_events_and_miss_reset();
    test_stage_end_cancels_unreleased_and_blocks_simultaneous_auto();
    test_last_airborne_ring_gets_full_landing_and_result_once();
    test_stars_progress_and_result_navigation();
    test_pause_exit_preserves_only_completed_progress();
    test_campaign_records_do_not_replace_the_legacy_ten_ring_best();
    test_spawn_bags_speed_and_cap();
    test_mapping_saturation_and_progress_validation();
    test_initial_brand_guarantee_and_closest_visible_hit();
    puts("Marvy six-level campaign, deadlines, progress and spawn tests: PASS");
    return 0;
}
