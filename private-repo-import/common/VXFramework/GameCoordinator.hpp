//
//  GameCoordinator.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 07/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

#pragma once

// C++
#include <vector>
#include <stack>

#include "State.hpp"
#include "VXSize.hpp"
#include "VXNode.hpp"
#include "VXGameRenderer.hpp"
#include "VXPopupEditText.hpp"
#include "VXGame.hpp"
#include "VXNotifications.hpp"
#include "VXGame.hpp"
#include "inputs.h"

// VXNetworking
#include "VXGameClient.hpp"

// VXGameServer
#include "VXGameServer.hpp"

namespace vx {

enum class PlaySubState {
    featuredState,
    popularState,
    newState,
    mineState
};

enum class BuildSubState {
    worldsState,
    itemsState
};

enum class WorldEditorState {
    none,
    loadingEditor,
    editing,
    loadingWorld,
    testingWorld,
    exiting,
};

typedef std::function<void(int status)> LoadCallback;
typedef std::function<void(void)> UnloadCallback;

///
class GameCoordinator final :
public TickListener,
public NotificationListener {
    
public:
    static const std::string ENV_FLAG_WORLD_ID;
    static const std::string ENV_FLAG_SCRIPT;

    ///
    static GameCoordinator *getInstance();
    
    /// Destructor
    ~GameCoordinator() override;

    // state utils
    
    ///
    static bool isPlayerGameState(const State::Value& value);
    
    /// Indicates whether the mouse cursor should be hidden.
    /// This is used by platform-specific interfaces (their tick function),
    /// to hide/show the cursor based on this function's return value.
    static bool shouldMouseCursorBeHidden();
    
    /// _state accessors
    State_SharedPtr getState();
    void setState(const State::Value& newState);
    void setState(State_SharedPtr newState);
    void applyScheduledState();
    
    void setupCodeEditor(std::string worldID, std::string code, bool readOnly);

    ///
    void initCurrentState();
    
    // returns root node that should be displayed, depending on state.
    const Node_SharedPtr& getCurrentRootNode();
    
    // returns ideal loading message based on general state
    // (and loaded game state eventually too)
    std::string getIdealLoadingMessage();
    
    void quickGameJoin(const std::string& gameID,
                       const bool isAuthor,
                       const bool gameDevMode,
                       const bool &useLocalServer);

    void exitWorldEditor();

    void loadHome(const std::string serverAddr,
                  const bool gameDevMode, 
                  const bool isAuthor,
                  std::unordered_map<std::string,std::string> env);

    ///
    void launchLocalItemEditor(const std::string& repo, const std::string& name, const std::string& itemCategory);
    
    //
    bool hasEnvironmentToLaunch();
    
    // Launches world if _envToLaunch contains sufficient information
    void launchEnvironment();
    
    //
    void loadingWorldStateChange();
    
    // puts background game in front, hiding CPP UI,
    // showing background's game Lua UI.
    void showBackgroundGame();
    // puts background game back underneath of CPP UI
    void hideBackgroundGame();

    bool isBackgroundGameShown() const;
    
    void showKeyboard(Size keyboardSize, float duration);
    void hideKeyboard(float duration);
    
    Size keyboardSize();
    
    /// Loads a game (other than the menu background game)
    /// itemsToLoad: optional, can be used to load specific items,
    /// even when in "local server and local resources" mode.
    void loadWorld(GameMode gameMode,
                   GameData_SharedPtr data,
                   WorldFetchInfo_SharedPtr fetchInfo,
                   const std::string serverAddr,
                   const bool gameDevMode,
                   const bool isAuthor,
                   std::vector<std::string> itemsToLoad,
                   std::unordered_map<std::string,std::string> env);

    ///
    void didRegisterForRemoteNotifications(const std::string& tokenTypeStr,
                                           const std::string& tokenValue);

    ///
    void retryPushTokenUploadIfNeeded();
    
    ///
    std::vector<std::string> gameResourcesToDownload;

    void addPreviousCommand(std::string command);
    /// returns a command done in the past, n = 1 is the last one
    const std::string& getPreviousCommand(size_t n);
    size_t getNbPreviousCommands();
    
    ///
    void logout(bool quit);
    
    ///
    void showPopup(Popup_SharedPtr popup);
    
    /// Get and remove popup to show.
    /// Returns nullptr if there isn't any popup to show.
    Popup_SharedPtr popPopupToShow();
    
    bool isPopupShown();
    
    /// because we currently need to call tick manually in the CI
    /// adding function instead of making tick public to reduce
    /// risk of abuse
    void tickForCI(double dt);
    
    void setSystemFileModalOpened(bool isOpened);
    bool isSystemFileModalOpened();

    /// returns menu background game or loaded game
    /// depending on their states.
    /// if loadedGame is not NULL and ready for rendering -> returns loadedGame
    /// otherwise, returns menu background game.
    Game_SharedPtr getActiveGame();

    ///
    const Game_SharedPtr& getLoadedWorld();
    
    ///
    const Game_SharedPtr& getLoadingWorld();

    ///
    const Game_SharedPtr& getEditorWorld();

    ///
    void worldEditorPlay();
    void worldEditorStop();

    ///
    void didReceiveNotification(const vx::Notification &notification) override;
    
    ///
    void setEnvironmentToLaunch(const std::unordered_map<std::string,std::string>& environment);
    
private:
    
    ///
    void _resetMainMenuStates();
    
    ///
    void _startupLoading();
    
    ///
    void tick(const double dt) override;
    
    ///
    static GameCoordinator *_instance;
    
    ///
    GameCoordinator();
    
    /// Current state = _states.top
    std::stack<State_SharedPtr> _states;
    
    ///
    State_SharedPtr _scheduledState;

    /// root node that should be displayed
    Node_SharedPtr _currentRootNode;
    /// state value when _currentRootNode was set
    State_SharedPtr _currentRootNodeState;

    RootNodeCodeEditor_SharedPtr _codeEditorNode;

    /// Filepath to background game script
    const char* _backgroundGameFilepath;
    std::string _backgroundGameID;
    std::mutex _backgroundGameMutex;
    
    ///
    std::mutex _stateMutex;

    bool _isBackgroundGameShown;
    
    Popup_SharedPtr _popupToShow;
    Popup_SharedPtr _quitResumePopup;

    Size _keyboardSize;

    std::list<std::string> _previousCommands;

    /// Called after resetting a game
    void setGamePointers(CGame *g);
    
    // WORLD SLOTS
    Game_SharedPtr _loadingWorld;
    Game_SharedPtr _loadedWorld;
    WorldEditorState _worldEditorState;
    Game_SharedPtr _editorWorld;

    bool _systemFileModalOpened;

    // Environnement to launch
    std::unordered_map<std::string,std::string> _envToLaunch;
    
    typedef struct {
        GameMode gameMode;
        GameData_SharedPtr data;
        WorldFetchInfo_SharedPtr fetchInfo;
        std::string serverAddr;
        bool gameDevMode;
        bool isAuthor;
        std::vector<std::string> itemsToLoad;
        std::unordered_map<std::string,std::string> env;
    } GameInfo;
    GameInfo *_lastLoadedGameInfo;
    
    //
    KeyboardInputListener *_keyboardInputListener;
    PointerEventListener *_pointerEventListener;
    
    std::string _worldIDToEdit;
    std::string _codeToEdit;
};

} // vx namespace
