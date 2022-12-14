#include "ReplayPlayer.h"
#include "bools.h"
#include "PlayLayer.h"
#include "Hacks.h"
#include "ctime"
#include "fmod.hpp"

extern struct HacksStr hacks;
FMOD::System *sys;
std::vector<FMOD::Sound *> clicks, releases, mediumclicks;
FMOD::Channel *channel = 0, *channel2 = 0;

int Hacks::amountOfClicks = 0;
int Hacks::amountOfMediumClicks = 0;
int Hacks::amountOfReleases = 0;

double oldTime = 0;

uint32_t ReplayPlayer::GetFrame()
{
    gd::PlayLayer *pl = gd::GameManager::sharedState()->getPlayLayer();
    if (pl)
        return static_cast<uint32_t>((float)pl->m_time * hacks.fps) + frameOffset;
    else
        return -1;
}

void ReplayPlayer::StartPlaying(gd::PlayLayer *playLayer)
{
    UpdateFrameOffset();
    if (IsRecording())
    {
        replay.fps = hacks.fps;
        Hacks::FPSBypass(replay.fps);
    }
    actionIndex = 0;
}

ReplayPlayer::ReplayPlayer()
{
    srand(time(NULL));
    FMOD::System_Create(&sys);
    sys->init(32, FMOD_INIT_NORMAL, nullptr);

    clicks.clear();
    releases.clear();
    mediumclicks.clear();

    bool exist = std::filesystem::exists("GDMenu/clicks/clicks/1.wav");
    uint32_t i = 1;
    while (exist)
    {
        auto filestr = "GDMenu/clicks/clicks/" + std::to_string(i) + ".wav";
        FMOD::Sound *s;
        sys->createSound(filestr.c_str(), FMOD_LOOP_OFF, 0, &s);
        if (s)
            clicks.push_back(s);
        i++;
        filestr = "GDMenu/clicks/clicks/" + std::to_string(i) + ".wav";
        exist = std::filesystem::exists(filestr);
    }
    Hacks::amountOfClicks = clicks.size();

    exist = std::filesystem::exists("GDMenu/clicks/releases/1.wav");
    i = 1;
    while (exist)
    {
        auto filestr = "GDMenu/clicks/releases/" + std::to_string(i) + ".wav";
        FMOD::Sound *s;
        sys->createSound(filestr.c_str(), FMOD_LOOP_OFF, 0, &s);
        if (s)
            releases.push_back(s);
        i++;
        filestr = "GDMenu/clicks/releases/" + std::to_string(i) + ".wav";
        exist = std::filesystem::exists(filestr);
    }
    Hacks::amountOfReleases = releases.size();

    exist = std::filesystem::exists("GDMenu/clicks/mediumclicks/1.wav");
    i = 1;
    while (exist)
    {
        auto filestr = "GDMenu/clicks/mediumclicks/" + std::to_string(i) + ".wav";
        FMOD::Sound *s;
        sys->createSound(filestr.c_str(), FMOD_LOOP_OFF, 0, &s);
        if (s)
            mediumclicks.push_back(s);
        i++;
        filestr = "GDMenu/clicks/mediumclicks/" + std::to_string(i) + ".wav";
        exist = std::filesystem::exists(filestr);
    }
    Hacks::amountOfMediumClicks = mediumclicks.size();
}

void ReplayPlayer::ToggleRecording()
{
    playing = false;
    recording = !recording;

    if (replay.GetActionsSize() > 0 && IsRecording() && hacks.actionStart <= 0)
    {
        replay.ClearActions();
    }
}

void ReplayPlayer::TogglePlaying()
{
    recording = false;
    playing = !playing;
    Hacks::FPSBypass(hacks.fps);
    Hacks::level["mods"][24]["toggle"] = true;
    Hacks::ToggleJSONHack(Hacks::level, 24, false);
}

void ReplayPlayer::Reset(gd::PlayLayer *playLayer)
{
    oldTime = 0;
    UpdateFrameOffset();
    actionIndex = hacks.actionStart;
    bool addedAction = false;

    bool hasCheckpoint = playLayer->m_checkpoints->count() > 0;
    const auto checkpoint = practice.GetLast();

    if (!hasCheckpoint)
    {
        frameOffset = 0;
    }
    else
    {
        frameOffset = checkpoint.frameOffset;
    }

    if (IsRecording())
    {
        replay.RemoveActionsAfter(GetFrame());
        const auto &actions = replay.getActions();
        bool holding = playLayer->m_pPlayer1->m_isHolding;

        Action ac;
        if (!actions.empty())
        {
            ac = actions.back();
            int i = actions.size();
            while (ac.dummy && i > 0)
            {
                i--;
                ac = actions[i];
            }
        }

        if ((holding && actions.empty()) || (!actions.empty() && ac.press != holding && !ac.dummy))
        {
            RecordAction(holding, playLayer->m_pPlayer1, true, false);
            addedAction = true;
            if (playLayer->m_bIsDualMode)
                RecordAction(holding, playLayer->m_pPlayer2, false, false);
            if (holding)
            {
                PlayLayer::releaseButton(playLayer->m_pPlayer1, 0);
                PlayLayer::pushButton(playLayer->m_pPlayer1, 0);
                playLayer->m_pPlayer1->m_hasJustHeld = true;
                if (playLayer->m_bIsDualMode)
                {
                    PlayLayer::releaseButton(playLayer->m_pPlayer2, 0);
                    PlayLayer::pushButton(playLayer->m_pPlayer2, 0);
                    playLayer->m_pPlayer2->m_hasJustHeld = true;
                }
            }
        }
        else if (!actions.empty() && holding && hasCheckpoint && ac.press && checkpoint.p1.buffer && !ac.dummy)
        {
            PlayLayer::releaseButton(playLayer->m_pPlayer1, 0);
            PlayLayer::pushButton(playLayer->m_pPlayer1, 0);
            if (playLayer->m_bIsDualMode)
            {
                PlayLayer::releaseButton(playLayer->m_pPlayer2, 0);
                PlayLayer::pushButton(playLayer->m_pPlayer2, 0);
            }
        }
    }
    else if (IsPlaying())
    {
        playLayer->releaseButton(0, false);
        playLayer->releaseButton(0, true);
    }

    if (IsRecording() || hacks.fixPractice)
        practice.ApplyCheckpoint(addedAction);
}

void ReplayPlayer::Load(std::string name)
{
    replay.Load(name);
}

void ReplayPlayer::Update(gd::PlayLayer *playLayer)
{
    sys->update();

    if (replay.fps != hacks.fps)
    {
        hacks.fps = replay.fps;
        Hacks::FPSBypass(hacks.fps);
    }

    if (!IsPlaying() || actionIndex >= replay.getActions().size() || replay.getActions().size() <= 0)
        return;

    size_t limit = 1;
    if (actionIndex + 1 < replay.getActions().size() && replay.getActions()[actionIndex + 1].player2)
        limit = 2;

    for (size_t i = 0; i < limit; i++)
    {
        auto ac = replay.getActions()[actionIndex];
        if (GetFrame() >= ac.frame || playLayer->m_pPlayer1->m_position.x >= ac.px)
        {
            if (!ac.player2)
            {
                playLayer->m_pPlayer1->m_position.x = ac.px;
                playLayer->m_pPlayer1->m_yAccel = ac.yAccel;
                playLayer->m_pPlayer1->m_position.y = ac.py;
            }
            else
            {
                playLayer->m_pPlayer2->m_position.x = ac.px;
                playLayer->m_pPlayer2->m_yAccel = ac.yAccel;
                playLayer->m_pPlayer2->m_position.y = ac.py;
            }

            if (!ac.dummy)
            {
                float v;
                float p;
                double t;
                uint16_t rc, rr, rmc;

                if (hacks.clickbot && clicks.size() > 0 && releases.size() > 0)
                {
                    rc = rand() % clicks.size();
                    rr = rand() % releases.size();
                    rmc = rand() % mediumclicks.size();
                    p = hacks.minPitch + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (hacks.maxPitch - hacks.minPitch)));
                    t = playLayer->m_time - oldTime;
                    v = t >= hacks.playMediumClicksAt ? 0.5f * ((float)t * 6) * hacks.baseVolume : 0.6f * hacks.baseVolume;
                    if (v > 0.5f * hacks.baseVolume)
                        v = 0.5f * hacks.baseVolume;
                }
                if (ac.press)
                {
                    if (hacks.clickbot && t > hacks.minTimeDifference && !ac.player2 && releases.size() > 0 && clicks.size() > 0)
                    {
                        sys->playSound(t > 0.0 && t < hacks.playMediumClicksAt && mediumclicks.size() > 0 ? mediumclicks[rmc] : clicks[rc], nullptr, false, &channel);
                        channel->setPitch(p);
                        channel->setVolume(v);
                    }
                    oldTime = playLayer->m_time;
                    //ac.player2 ? playLayer->m_pPlayer2->pushButton(0) : playLayer->m_pPlayer1->pushButton(0);
                    PlayLayer::isBot = true;
                    ac.player2 ? PlayLayer::pushButtonHook(playLayer->m_pPlayer2, 0, 0) : PlayLayer::pushButtonHook(playLayer->m_pPlayer1, 0, 0);
                    PlayLayer::isBot = false;
                }
                else
                {
                    if (hacks.clickbot && !ac.player2 && releases.size() > 0 && clicks.size() > 0)
                    {
                        sys->playSound(releases[rr], nullptr, false, &channel);
                        channel2->setPitch(p);
                        channel2->setVolume(v + 0.5f);
                    }
                    oldTime = playLayer->m_time;
                    //ac.player2 ? playLayer->m_pPlayer2->releaseButton(0) : playLayer->m_pPlayer1->releaseButton(0);
                    PlayLayer::isBot = true;
                    ac.player2 ? PlayLayer::releaseButtonHook(playLayer->m_pPlayer2, 0, 0) : PlayLayer::releaseButtonHook(playLayer->m_pPlayer1, 0, 0);
                    PlayLayer::isBot = false;
                }
            }

            ++actionIndex;
        }
    }
}

void ReplayPlayer::UpdateFrameOffset()
{
    frameOffset = GetPractice().GetLast().frameOffset;
}

void ReplayPlayer::RecordAction(bool press, gd::PlayerObject *pl, bool player1, bool dummy)
{
    Action a;
    a.player2 = !player1;
    a.frame = GetFrame();
    a.press = press;
    a.yAccel = pl->m_yAccel;
    a.dummy = dummy;
    a.px = pl->m_position.x;
    a.py = pl->m_position.y;
    replay.AddAction(a);
}