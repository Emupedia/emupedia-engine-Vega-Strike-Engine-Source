/// Functions for python modules
///
#include "cmd/script/mission.h"
#include "universe_util.h"
#include "universe.h"
#include "cmd/unit.h"
#include "cmd/unit_factory.h"    //for UnitFactory::getMasterPartList()
#include "cmd/collection.h"
#include "star_system.h"
#include <string>
#include "cmd/music.h"
#include "audiolib.h"
#include "gfx/animation.h"
#include "lin_time.h"
#include "load_mission.h"
#include "config_xml.h"
#include "vs_globals.h"
#include "gfx/particle.h"
#include "cmd/base.h"
#include "options.h"
#include "universe.h"
#include "savegame.h"


extern unsigned int AddAnimation( const QVector &pos,
                                  const float size,
                                  bool mvolatile,
                                  const std::string &name,
                                  float percentgrow );
extern void RespawnNow( Cockpit *cp );
extern void TerminateCurrentBase( void );
extern void SetStarSystemLoading( bool value );
extern bool GetStarSystemLoading();
extern void bootstrap_draw( const std::string &message, Animation *newSplashScreen );
extern Animation * GetSplashScreen();
extern const vector< string >& ParseDestinations( const string &value );

using std::string;
//less to write
#define activeSys _Universe->activeStarSystem()
void ClientServerSetLightContext( int lightcontext )
{
    GFXSetLightContext( lightcontext );
}

namespace UniverseUtil
{
void playVictoryTune()
{
    muzak->GotoSong( game_options.missionvictorysong );
}
int musicAddList( string str )
{
    return muzak->Addlist( str.c_str() );
}
void musicLayerSkip( int layer )
{
    muzak->Skip( layer );
}
void musicLayerStop( int layer )
{
    muzak->Stop( layer );
}
void musicLayerPlaySong( string str, int layer )
{
    muzak->GotoSong( str, layer );
}
void musicLayerPlayList( int which, int layer )
{
    if (which != -1)
        muzak->SkipRandSong( which, layer );
}
void musicLayerLoopList( int numloops, int layer )
{
    muzak->SetLoops( numloops, layer );
}
void musicLayerSetSoftVolume( float vol, float latency_override, int layer )
{
    Music::SetVolume( vol, layer, false, latency_override );
}
void musicLayerSetHardVolume( float vol, int layer )
{
    Music::SetVolume( vol, layer, true );
}
void musicSetSoftVolume( float vol, float latency_override )
{
    musicLayerSetSoftVolume( vol, latency_override, -1 );
}
void musicSetHardVolume( float vol )
{
    musicLayerSetHardVolume( vol, -1 );
}
void musicMute( bool stopSound )
{
    muzak->Mute( stopSound );
}
void playSound( string soundName, QVector loc, Vector speed )
{
    int sound = AUDCreateSoundWAV( soundName, false );
    AUDAdjustSound( sound, loc, speed );
    AUDStartPlaying( sound );
    AUDDeleteSound( sound );
}
void playSoundCockpit( string soundName )
{
    int sound = AUDCreateSoundWAV( soundName, false );
    AUDStartPlaying( sound );
    AUDDeleteSound( sound );
}
void StopAllSounds( void )
{
    AUDStopAllSounds();
}
void cacheAnimation( string aniName )
{
    static vector< Animation* >anis;
    anis.push_back( new Animation( aniName.c_str() ) );
}
void playAnimation( string aniName, QVector loc, float size )
{
    AddAnimation( loc, size, true, aniName, 1 );
}
void playAnimationGrow( string aniName, QVector loc, float size, float growpercent )
{
    AddAnimation( loc, size, true, aniName, growpercent );
}
unsigned int getCurrentPlayer()
{
    return _Universe->CurrentCockpit();
}
unsigned int maxMissions()
{
    return game_options.max_missions;
}
void addParticle( QVector loc, Vector velocity, Vector color, float size )
{
    ParticlePoint p;
    p.loc = loc;
    p.col = color;
    particleTrail.AddParticle( p, velocity, size );
}

void loadGame( const string &savename )
{
    Cockpit *cockpit = _Universe->AccessCockpit();
    Unit    *player  = cockpit->GetParent();
    UniverseUtil::setCurrentSaveGame( savename );
    if (player) {

            player->Kill();
    }
    RespawnNow( cockpit );
    globalWindowManager().shutDown();
    TerminateCurrentBase();
}

void saveGame( const string &savename )
{

        UniverseUtil::setCurrentSaveGame( savename );
        WriteSaveGame( _Universe->AccessCockpit(), false );

}

void showSplashScreen( const string &filename )
{
    static Animation *curSplash = 0;
    if ( !filename.empty() ) {
        if (curSplash)
            delete curSplash;
        curSplash = new Animation( filename.c_str(), 0 );
    } else if ( !curSplash && !GetSplashScreen() ) {
        static std::vector< std::string >s = ParseDestinations( game_options.splash_screen );
        int snum = time( NULL )%s.size();
        curSplash = new Animation( s[snum].c_str(), 0 );
    }
    SetStarSystemLoading( true );
    bootstrap_draw( "Loading...", curSplash );
}

void showSplashMessage( const string &text )
{
    bootstrap_draw( text, 0 );
}

void showSplashProgress( float progress ) //DELETE ?
{
    //Unimplemented
}

void hideSplashScreen()
{
    SetStarSystemLoading( false );
}

bool isSplashScreenShowing()
{
    return GetStarSystemLoading();
}

void sendCustom( int cp, string cmd, string args, string id )
{
    if ( cp < 0 || cp >= static_cast<int>(_Universe->numPlayers()) ) {
        fprintf( stderr, "sendCustom %s with invalid player %d\n", cmd.c_str(), cp );
        return;
    }
        receivedCustom( cp, true, cmd, args, id );
}
}

#undef activeSys

