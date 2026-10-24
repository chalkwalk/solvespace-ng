//-----------------------------------------------------------------------------
// The SDL3 + Dear ImGui platform backend (solvespace-ng).
//
// One SDL window, one GL context. The graphics window draws into the default framebuffer;
// TOOL windows (the property browser) draw into offscreen framebuffers shown as ImGui panels.
//-----------------------------------------------------------------------------
#include <atomic>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#if defined(__APPLE__)
// macOS gives us a legacy GL 2.1 context, which has no vertex array objects.
#   include "imgui_impl_opengl2.h"
#   define ImGuiRenderer_Init(glsl)       ImGui_ImplOpenGL2_Init()
#   define ImGuiRenderer_Shutdown         ImGui_ImplOpenGL2_Shutdown
#   define ImGuiRenderer_NewFrame         ImGui_ImplOpenGL2_NewFrame
#   define ImGuiRenderer_RenderDrawData   ImGui_ImplOpenGL2_RenderDrawData
#else
#   include "imgui_impl_opengl3.h"
#   define ImGuiRenderer_Init(glsl)       ImGui_ImplOpenGL3_Init(glsl)
#   define ImGuiRenderer_Shutdown         ImGui_ImplOpenGL3_Shutdown
#   define ImGuiRenderer_NewFrame         ImGui_ImplOpenGL3_NewFrame
#   define ImGuiRenderer_RenderDrawData   ImGui_ImplOpenGL3_RenderDrawData
#endif
#include "solvespace.h"
#include "render/gl3shader.h"
#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#endif
#include "platform/sdl/settingsfile.h"
#include "platform/sdl/menumodel.h"
#include "platform/sdl/timerregistry.h"
#include "platform/sdl/dialoghelpers.h"
#include "platform/sdl/debugscript.h"
#include "platform/sdl/deferredqueue.h"
#include "platform/sdl/keychord.h"
#include "platform/sdl/uidriver.h"
#include "platform/sdl/itemregistry.h"
#include "platform/sdl/icontextures.h"
#include "platform/sdl/touchgestures.h"
#include "ui/iconraster.h"
#include "ui/keyrouter.h"
#include "ui/menulayout.h"
#include "ui/panels.h"
#include "ui/viewcube.h"
#include "ui/uiactions.h"
#include "ui/coreactions.h"
#include "ui/ngui.h"
#include "commandnames.h"

namespace SolveSpace {
namespace Platform {

class WindowImplSdl;
static void ReleaseFramebuffer(WindowImplSdl *w);

static SDL_Window    *gSdlWindow   = nullptr;
static SDL_GLContext  gGlContext   = nullptr;
static Uint32         gTimerEvent  = 0;
static Uint32         gWakeEvent   = 0; // a solid finished on the kernel's worker (regen.cpp)
static bool           gQuit        = false;
static bool           gNeedsFrame  = true;
static std::weak_ptr<WindowImplSdl>              gMainWindow;
static bool           gShowMenuBar = true;
static Ui::KeyRouter  gKeyRouter;
// UI callbacks into the core run after the frame, never while ImGui is building one: the core
// may open a dialog (which renders frames itself) or rebuild the menus being drawn.
static DeferredQueue  gDeferred;
// > 0 while a file dialog or context menu runs its nested loop; the canvas and keymap are inert.
static int            gModalDepth     = 0;
static bool           gCloseRequested = false;

static void DrawMainMenuBar();
static bool RouteKeyDown(const SDL_Event &e);
static void RenderFrame();
static void RunNestedLoop(const std::function<bool()> &done);
static void DrawActivePopup();
static void LoadDebugScript();

namespace Driver { void Settle(unsigned horizonMs); }

// Test driver mode (uidriver.h): dialogs never block and held keys come from the driver.
static bool                    gDriver          = false;
static SDL_Keymod              gDriverMods      = SDL_KMOD_NONE;
static float                   gDriverX         = 0, gDriverY = 0;
static SDL_MouseButtonFlags    gDriverButtons   = 0;
static std::vector<Driver::DialogRecord>                                     gDialogs;
static std::function<MessageDialog::Response(const Driver::DialogRecord &)> gMessagePolicy;
static std::deque<Path>                  gQueuedFiles;
static std::deque<std::function<void()>> gModalSteps;
static int                               gSettleTimeouts = 0;

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    fprintf(stderr, "fatal error: %s\n", message.c_str());
    if(!gDriver) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SolveSpace: fatal error",
                             message.c_str(), gSdlWindow);
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

static Path gConfigDir; // the test driver's; empty = the user's

#if defined(__EMSCRIPTEN__)
// The web build's files and settings: IDBFS keeps this folder in the browser's IndexedDB
// between visits. It is read once at start, and written back after files are saved or opened
// and when the settings change (PersistWebStorage).
static const char *WebFolder = "/data";

static void MountWebStorage() {
    EM_ASM({
        try { FS.mkdir(UTF8ToString($0)); } catch(e) {}
        FS.mount(IDBFS, {}, UTF8ToString($0));
        Module.ngLoaded = false;
        FS.syncfs(true, function(err) {
            if(err) console.log('SolveSpace NG: stored files not read:', err);
            Module.ngLoaded = true;
        });
    }, WebFolder);
    while(!EM_ASM_INT({ return Module.ngLoaded ? 1 : 0; })) emscripten_sleep(10);
}

// Writes the folder back to IndexedDB, once more after a write still under way finishes.
static void PersistWebStorage() {
    EM_ASM({
        if(Module.ngPersisting) {
            Module.ngPersistAgain = true;
            return;
        }
        var persist = function() {
            Module.ngPersisting   = true;
            Module.ngPersistAgain = false;
            FS.syncfs(false, function(err) {
                if(err) console.log('SolveSpace NG: files not stored:', err);
                Module.ngPersisting = false;
                if(Module.ngPersistAgain) persist();
            });
        };
        persist();
    });
}
#endif

static Path PrefDir() {
    if(!gConfigDir.IsEmpty()) return gConfigDir;
    if(const char *dir = getenv("SOLVESPACE_NG_CONFIG_DIR")) return Path::From(dir);
#if defined(__EMSCRIPTEN__)
    Path web = Path::From(WebFolder).Join("settings");
    std::error_code ec;
    std::filesystem::create_directories(web.raw, ec);
    return web;
#else
    char *pref = SDL_GetPrefPath("solvespace", "solvespace-ng");
    Path dir = Path::From(pref ? pref : ".");
    SDL_free(pref);
    return dir;
#endif
}

static std::shared_ptr<SettingsFile> gSettings;

SettingsRef GetSettings() {
    if(!gSettings) {
        gSettings = std::make_shared<SettingsFile>(PrefDir().Join("settings.txt"));
        // NG's View > Show Toolbar shows the tool shelf, which starts hidden (AGENTS.md 5).
        if(gSettings->ThawBool("ShowToolbar", true) != gSettings->ThawBool("ShowToolbar", false)) {
            gSettings->FreezeBool("ShowToolbar", false);
        }
        // NG colours constraint glyphs by solve state unless turned off in Preferences.
        if(gSettings->ThawBool("ConstraintStateColors", true) !=
           gSettings->ThawBool("ConstraintStateColors", false)) {
            gSettings->FreezeBool("ConstraintStateColors", true);
        }
        // NG makes solids with Open CASCADE only, where it is built (AGENTS.md section 6); an
        // older choice of SolveSpace's kernel is dropped.
        int kernel = KernelAvailable(KernelKind::OCCT) ? 1 : 0;
        if(gSettings->ThawInt("Kernel", -1) != kernel) gSettings->FreezeInt("Kernel", kernel);
    }
    return gSettings;
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

// Never destroyed: timers owned by the global SolveSpaceUI are destroyed during static
// destruction, after a namespace-scope registry would already be gone.
static TimerRegistry &Timers() {
    static TimerRegistry *registry = new TimerRegistry;
    return *registry;
}

static Uint32 SDLCALL TimerCallback(void *userdata, SDL_TimerID, Uint32) {
    SDL_Event ev  = {};
    ev.type       = gTimerEvent;
    ev.user.data1 = userdata; // the serial, smuggled as a pointer-sized integer
    SDL_PushEvent(&ev);
    return 0;
}

class TimerImplSdl final : public Timer, public std::enable_shared_from_this<TimerImplSdl> {
public:
    SDL_TimerID id     = 0;
    uint64_t    serial = 0;

    ~TimerImplSdl() override { Cancel(); }

    void Cancel() {
        if(id) {
            SDL_RemoveTimer(id);
            id = 0;
        }
        if(serial) {
            Timers().Cancel(serial);
            serial = 0;
        }
    }

    void RunAfter(unsigned milliseconds) override {
        Cancel();
        serial = Timers().Arm(weak_from_this(), SDL_GetTicks() + milliseconds);
        void *token = (void *)(uintptr_t)serial;
        if(milliseconds == 0) {
            TimerCallback(token, 0, 0);
        } else {
            id = SDL_AddTimer(milliseconds, TimerCallback, token);
        }
    }
};

TimerRef CreateTimer() {
    return std::make_shared<TimerImplSdl>();
}

static void HandleTimerEvent(const SDL_Event &ev) {
    uint64_t serial = (uint64_t)(uintptr_t)ev.user.data1;
    std::shared_ptr<Timer> timer = Timers().Take(serial);
    if(!timer) return;
    auto sdlTimer = std::static_pointer_cast<TimerImplSdl>(timer);
    if(sdlTimer->serial != serial) return;
    sdlTimer->id     = 0;
    sdlTimer->serial = 0;
    if(sdlTimer->onTimeout) sdlTimer->onTimeout();
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

static MenuSdl *gActivePopup = nullptr;
static bool     gPopupOpened = false;

MenuRef CreateMenu() {
    auto menu = std::make_shared<MenuSdl>();
    menu->onPopUp = [](MenuSdl *m) {
        Ui::ApplyContextMenuLayout(*m, !SS.GW.selection.IsEmpty());
        gActivePopup = m;
        gPopupOpened = false;
        gNeedsFrame  = true;
        RunNestedLoop([] { return gActivePopup == nullptr; });
        m->popupRequested = false;
    };
    return menu;
}

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    // A fresh bar each time, as the retired GTK frontend did: the core installs it with SetMenuBar.
    *unique = false;
    return std::make_shared<MenuBarSdl>();
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class WindowImplSdl final : public Window {
public:
    Kind        kind;
    std::string title;
    bool        visible = true;
    double      width = 800, height = 600; // logical pixels
    Cursor      cursor = Cursor::POINTER;
    MenuBarRef  menuBar;
    // Editor overlay (dimension/value entry), positioned in this window's coordinates.
    bool        editorVisible = false, editorFocusPending = false;
    double      editorX = 0, editorY = 0, editorFontHeight = 0, editorMinWidth = 0;
    bool        editorMonospace = false;
    std::string editorText;
    // Tooltip
    std::string tooltipText;
    double      tooltipX = 0, tooltipY = 0, tooltipW = 0, tooltipH = 0;
    // Scrollbar (TOOL windows)
    bool        scrollbarVisible = false;
    double      scrollMin = 0, scrollMax = 0, scrollPage = 0, scrollPos = 0;
    // Offscreen target (TOOL windows).
    unsigned    fbo = 0, fboColor = 0, fboDepth = 0;
    int         fboWidth = 0, fboHeight = 0;
    bool        wasHovered = false;
    int         buttonsDown = 0; // presses on the tool-window image awaiting their release

    explicit WindowImplSdl(Kind kind) : kind(kind) {}
    ~WindowImplSdl() override { ReleaseFramebuffer(this); }

    double GetPixelDensity() override {
        float scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(gSdlWindow));
        return 96.0 * (scale > 0 ? scale : 1.0f);
    }
    double GetDevicePixelRatio() override {
        float d = SDL_GetWindowPixelDensity(gSdlWindow);
        return d > 0 ? d : 1.0;
    }

    bool IsVisible() override { return visible; }
    void SetVisible(bool v) override {
        visible = v;
        if(kind == Kind::TOPLEVEL) {
            if(v) SDL_ShowWindow(gSdlWindow); else SDL_HideWindow(gSdlWindow);
        }
        Invalidate();
    }
    void Focus() override {
        if(kind == Kind::TOPLEVEL) SDL_RaiseWindow(gSdlWindow);
    }

    bool IsFullScreen() override {
        return (SDL_GetWindowFlags(gSdlWindow) & SDL_WINDOW_FULLSCREEN) != 0;
    }
    void SetFullScreen(bool fs) override {
        SDL_SetWindowFullscreen(gSdlWindow, fs);
        if(onFullScreen) onFullScreen(fs);
    }

    void SetTitle(const std::string &t) override {
        title = t;
        if(kind == Kind::TOPLEVEL) SDL_SetWindowTitle(gSdlWindow, t.c_str());
    }

    void SetMenuBar(MenuBarRef mb) override {
        // NG's layout of the core's menus (ui/menulayout.h).
        if(mb) Ui::ApplyMenuLayout(*std::static_pointer_cast<MenuBarSdl>(mb), Ui::ActionChecked);
        menuBar = mb;
    }

    void GetContentSize(double *w, double *h) override {
        if(kind == Kind::TOPLEVEL) {
            int iw, ih;
            SDL_GetWindowSize(gSdlWindow, &iw, &ih);
            width  = iw;
            height = ih;
        }
        *w = width;
        *h = height;
    }
    void SetMinContentSize(double w, double h) override {
        if(kind == Kind::TOPLEVEL) SDL_SetWindowMinimumSize(gSdlWindow, (int)w, (int)h);
    }

    void FreezePosition(SettingsRef settings, const std::string &key) override {
        if(kind != Kind::TOPLEVEL) {
            settings->FreezeBool(key + "_Visible", visible);
            return;
        }
        int x, y, w, h;
        SDL_GetWindowPosition(gSdlWindow, &x, &y);
        SDL_GetWindowSize(gSdlWindow, &w, &h);
        settings->FreezeInt(key + "_Left", (uint32_t)x);
        settings->FreezeInt(key + "_Top", (uint32_t)y);
        settings->FreezeInt(key + "_Width", (uint32_t)w);
        settings->FreezeInt(key + "_Height", (uint32_t)h);
        settings->FreezeBool(key + "_Maximized",
                             (SDL_GetWindowFlags(gSdlWindow) & SDL_WINDOW_MAXIMIZED) != 0);
    }
    void ThawPosition(SettingsRef settings, const std::string &key) override {
        if(kind != Kind::TOPLEVEL) {
            visible = settings->ThawBool(key + "_Visible", true);
            return;
        }
        int w = (int)settings->ThawInt(key + "_Width", 1280);
        int h = (int)settings->ThawInt(key + "_Height", 800);
        SDL_SetWindowSize(gSdlWindow, w, h);
        int x = (int)settings->ThawInt(key + "_Left", (uint32_t)SDL_WINDOWPOS_CENTERED);
        int y = (int)settings->ThawInt(key + "_Top", (uint32_t)SDL_WINDOWPOS_CENTERED);
        SDL_SetWindowPosition(gSdlWindow, x, y);
        if(settings->ThawBool(key + "_Maximized", false)) SDL_MaximizeWindow(gSdlWindow);
    }

    void SetCursor(Cursor c) override { cursor = c; }
    void SetTooltip(const std::string &text, double x, double y, double w, double h) override {
        tooltipText = text;
        tooltipX = x; tooltipY = y; tooltipW = w; tooltipH = h;
    }

    bool IsEditorVisible() override { return editorVisible; }
    void ShowEditor(double x, double y, double fontHeight, double minWidth, bool isMonospace,
                    const std::string &text) override {
        editorVisible = true;
        editorFocusPending = true;
        editorX = x; editorY = y; editorFontHeight = fontHeight; editorMinWidth = minWidth;
        editorMonospace = isMonospace;
        editorText = text;
        Invalidate();
    }
    void HideEditor() override {
        editorVisible = false;
        Invalidate();
    }

    void SetScrollbarVisible(bool v) override { scrollbarVisible = v; }
    void ConfigureScrollbar(double min, double max, double page) override {
        scrollMin = min; scrollMax = max; scrollPage = page;
    }
    double GetScrollbarPosition() override { return scrollPos; }
    void SetScrollbarPosition(double pos) override {
        scrollPos = std::max(scrollMin, std::min(pos, scrollMax - scrollPage));
    }

    void Invalidate() override { gNeedsFrame = true; }
};

static void InitSdlWindowAndImGui();

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    auto window = std::make_shared<WindowImplSdl>(kind);
    if(kind == Window::Kind::TOPLEVEL) {
        ssassert(gMainWindow.expired(), "only one TOPLEVEL window is supported");
        InitSdlWindowAndImGui();
        gMainWindow = window;
    } else {
        // The core's property browser (SS.TW). NG shows its screens as panels and never draws
        // this window (the floating browser was retired with the old frontends, M1c task 10).
        window->width  = 420;
        window->height = 640;
    }
    return window;
}

void Open3DConnexion() {}
void Close3DConnexion() {}
void Request3DConnexionEventsForWindow(WindowRef window) {}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplSdl final : public MessageDialog {
public:
    Type                                          type = Type::INFORMATION;
    std::string                                   title, message, description;
    std::vector<std::pair<std::string, Response>> buttons;
    int                                           defaultIndex = -1;

    void SetType(Type t) override { type = t; }
    void SetTitle(std::string t) override { title = t; }
    void SetMessage(std::string m) override { message = m; }
    void SetDescription(std::string d) override { description = d; }
    void AddButton(std::string label, Response response, bool isDefault) override {
        if(isDefault) defaultIndex = (int)buttons.size();
        buttons.push_back({ StripMnemonic(label), response });
    }

    Response RunModal() override {
        if(gDriver) return RunScripted();
        std::vector<SDL_MessageBoxButtonData> sdlButtons;
        for(size_t i = 0; i < buttons.size(); i++) {
            SDL_MessageBoxButtonData b = {};
            b.buttonID = (int)i;
            b.text     = buttons[i].first.c_str();
            if((int)i == defaultIndex) b.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
            if(buttons[i].second == Response::CANCEL) {
                b.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
            }
            sdlButtons.push_back(b);
        }
        std::string text = description.empty() ? message : message + "\n\n" + description;
        SDL_MessageBoxData data = {};
        data.flags = type == Type::ERROR   ? SDL_MESSAGEBOX_ERROR
                   : type == Type::WARNING ? SDL_MESSAGEBOX_WARNING
                                           : SDL_MESSAGEBOX_INFORMATION;
        data.window     = gSdlWindow;
        data.title      = title.c_str();
        data.message    = text.c_str();
        data.numbuttons = (int)sdlButtons.size();
        data.buttons    = sdlButtons.data();
        int pressed = -1;
        if(!SDL_ShowMessageBox(&data, &pressed) || pressed < 0) return Response::NONE;
        return buttons[pressed].second;
    }

    Response RunScripted() {
        Driver::DialogRecord rec;
        rec.kind    = "message";
        rec.title   = title;
        rec.message = description.empty() ? message : message + "\n\n" + description;
        for(const auto &b : buttons) rec.buttons.push_back(b.first);
        gDialogs.push_back(rec);
        if(gMessagePolicy) return gMessagePolicy(rec);
        if(defaultIndex >= 0) return buttons[defaultIndex].second;
        return buttons.empty() ? Response::NONE : buttons[0].second;
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplSdl>();
}

//-----------------------------------------------------------------------------
// Nested event loop (used by blocking file dialogs and context menus)
//-----------------------------------------------------------------------------

static void PumpEventsOnce(bool wait);
#if defined(__EMSCRIPTEN__)
static void TakeSoftKeyboardInput();
#endif

// Runs until done(), even if the app is asked to quit meanwhile (the request is deferred):
// a file dialog's callback must not outlive the RunModal frame that owns its result.
static void RunNestedLoop(const std::function<bool()> &done) {
    gModalDepth++;
    if(gDriver) {
        // Run the steps the test queued; with none, give up after a few idle frames.
        for(int idle = 0; !done();) {
            if(!gModalSteps.empty()) {
                Driver::Settle(/*horizonMs=*/0); // let the menu appear first
                auto step = std::move(gModalSteps.front());
                gModalSteps.pop_front();
                step();
            } else if(++idle > 10) {
                gDialogs.push_back({ "unanswered-modal", "", "a nested loop had no queued step" });
                gActivePopup = nullptr;
            } else {
                PumpEventsOnce(/*wait=*/false);
                RenderFrame();
            }
        }
        gModalDepth--;
        return;
    }
    while(!done()) {
        PumpEventsOnce(/*wait=*/true);
        RenderFrame();
    }
    gModalDepth--;
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

#if defined(__EMSCRIPTEN__)
// A browser has no file dialogs for SDL to show. Open copies a file picked with the browser's
// own picker into the web folder (WebFolder, kept between visits). Save asks for a name, and
// every file the app then writes there (one per part for Export Parts) is handed to the browser
// as a download, as is every later Save.
static std::vector<std::string> gWebDownloads;
// The folder's files and their times when a save dialog returned, to see which the save wrote.
static std::optional<std::map<std::string, std::filesystem::file_time_type>> gWebBeforeSave;

static std::map<std::string, std::filesystem::file_time_type> WebFolderFiles() {
    std::map<std::string, std::filesystem::file_time_type> files;
    std::error_code ec;
    for(const auto &entry : std::filesystem::directory_iterator(WebFolder, ec)) {
        if(entry.is_regular_file(ec)) files[entry.path().string()] = entry.last_write_time(ec);
    }
    return files;
}

static bool PickWebFile(Path *path) {
    EM_ASM({
        delete Module.ngPicked;
        Module.ngPickFile();
    });
    RunNestedLoop([] { return EM_ASM_INT({ return Module.ngPicked !== undefined; }) != 0; });
    int size = EM_ASM_INT({ return Module.ngPicked ? Module.ngPicked.data.length : -1; });
    if(size < 0) return false;
    char name[256];
    EM_ASM({ stringToUTF8(Module.ngPicked.name, $0, $1); }, name, (int)sizeof(name));
    std::string data(size, '\0');
    EM_ASM({
        HEAPU8.set(Module.ngPicked.data, $0);
        Module.ngPicked = null;
    }, data.data());
    std::string file = WebFileName(name);
    if(file.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(WebFolder, ec);
    *path = Path::From(WebFolder).Join(file);
    if(!WriteFile(*path, data)) return false;
    PersistWebStorage();
    return true;
}

static bool AskWebSaveName(const std::string &title, const std::vector<FileFilter> &filters,
                           Path *path) {
    std::string suggested = path->IsEmpty() ? "" : path->FileName();
    if(suggested.empty()) suggested = SuggestedSavePath("", "", filters);
    char answer[256];
    int ok = EM_ASM_INT({
        var name = prompt(UTF8ToString($0), UTF8ToString($1));
        if(name === null) return 0;
        stringToUTF8(name, $2, $3);
        return 1;
    }, title.empty() ? "Save as" : title.c_str(), suggested.c_str(), answer, (int)sizeof(answer));
    std::string file = ok ? WebFileName(answer) : "";
    if(file.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(WebFolder, ec);
    *path          = Path::From(EnsureExtension(Path::From(WebFolder).Join(file).raw, filters, 0));
    gWebBeforeSave = WebFolderFiles();
    return true;
}

// Runs between frames, after the save that asked for a name has written its files. An image
// export writes its file at the next redraw, so its download waits for that.
static void DownloadWebFiles() {
    if(!SS.screenshotFile.IsEmpty()) return;
    if(gWebBeforeSave) {
        for(const auto &[name, time] : WebFolderFiles()) {
            auto before = gWebBeforeSave->find(name);
            if(before == gWebBeforeSave->end() || before->second != time) {
                if(std::find(gWebDownloads.begin(), gWebDownloads.end(), name) ==
                   gWebDownloads.end()) {
                    gWebDownloads.push_back(name);
                }
            }
        }
        gWebBeforeSave.reset();
    }
    for(const std::string &name : gWebDownloads) {
        Path        path = Path::From(name);
        std::string data;
        if(!ReadFile(path, &data)) continue;
        EM_ASM({ Module.ngDownload(UTF8ToString($0), HEAPU8.slice($1, $1 + $2)); },
               path.FileName().c_str(), data.data(), (int)data.size());
    }
    if(!gWebDownloads.empty()) PersistWebStorage();
    gWebDownloads.clear();
}

// Settings are saved when the app quits, which a closed browser tab never does: save them
// every couple of seconds while they have changed.
static void SaveWebSettings() {
    static Uint64 last = 0;
    Uint64        now  = SDL_GetTicks();
    if(now - last < 2000) return;
    last = now;
    SS.SaveSettings(); // the preferences and recent files reach the settings only on request
    if(gSettings && gSettings->Dirty() && gSettings->Save()) PersistWebStorage();
}
#endif

class FileDialogImplSdl final : public FileDialog {
public:
    bool                    isSave;
    std::string             title;
    Path                    filename;
    Path                    folder;   // remembered by ThawChoices
    std::vector<FileFilter> filters;

    explicit FileDialogImplSdl(bool isSave) : isSave(isSave) {}

    void SetTitle(std::string t) override { title = t; }
    void SetCurrentName(std::string name) override { filename = filename.Parent().Join(name); }
    Path GetFilename() override { return filename; }
    void SetFilename(Path path) override { filename = path; }
    void SuggestFilename(Path path) override {
        if(!isSave) return; // as in the retired GTK frontend: suggestions only apply to save dialogs
        filename = Path::From(SuggestedSavePath(path.raw, folder.raw, filters));
    }
    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        filters.push_back({ name, extensions });
    }
    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        settings->FreezeString("Dialog_" + key + "_Folder", filename.Parent().raw);
    }
    void ThawChoices(SettingsRef settings, const std::string &key) override {
        folder = Path::From(settings->ThawString("Dialog_" + key + "_Folder", ""));
        if(!folder.IsEmpty() && filename.IsEmpty()) filename = folder.Join("");
    }

    bool RunModal() override {
        if(gDriver) {
            gDialogs.push_back({ isSave ? "save" : "open", title, filename.raw });
            if(gQueuedFiles.empty()) return false;
            filename = gQueuedFiles.front();
            gQueuedFiles.pop_front();
            return true;
        }
#if defined(__EMSCRIPTEN__)
        return isSave ? AskWebSaveName(title, filters, &filename) : PickWebFile(&filename);
#endif
        struct Result {
            std::atomic<bool> done{ false };
            std::mutex        mutex;
            std::string       path;
            int               filter = -1;
        } result;

        std::vector<std::string> patterns;
        for(const FileFilter &f : filters) patterns.push_back(SdlFilterPattern(f));
        std::vector<SDL_DialogFileFilter> sdlFilters;
        for(size_t i = 0; i < filters.size(); i++) {
            sdlFilters.push_back({ filters[i].name.c_str(), patterns[i].c_str() });
        }

        // SDL may invoke this on another thread.
        SDL_DialogFileCallback callback = [](void *userdata, const char *const *filelist,
                                             int filter) {
            Result *r = (Result *)userdata;
            {
                std::lock_guard<std::mutex> lock(r->mutex);
                if(filelist && filelist[0]) r->path = filelist[0];
                r->filter = filter;
            }
            r->done = true;
        };
        const char *location = filename.IsEmpty() ? nullptr : filename.raw.c_str();
        if(isSave) {
            SDL_ShowSaveFileDialog(callback, &result, gSdlWindow, sdlFilters.data(),
                                   (int)sdlFilters.size(), location);
        } else {
            SDL_ShowOpenFileDialog(callback, &result, gSdlWindow, sdlFilters.data(),
                                   (int)sdlFilters.size(), location, /*allow_many=*/false);
        }
        RunNestedLoop([&] { return result.done.load(); });

        std::lock_guard<std::mutex> lock(result.mutex);
        if(result.path.empty()) return false;
        std::string chosen = result.path;
        if(isSave) chosen = EnsureExtension(chosen, filters, result.filter);
        filename = Path::From(chosen);
        return true;
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplSdl>(/*isSave=*/false);
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplSdl>(/*isSave=*/true);
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Path> GetFontFiles() {
    std::vector<std::string> roots;
#if defined(_WIN32)
    if(const char *windir = getenv("WINDIR")) roots.push_back(std::string(windir) + "\\Fonts");
    if(const char *local = getenv("LOCALAPPDATA")) {
        roots.push_back(std::string(local) + "\\Microsoft\\Windows\\Fonts");
    }
#elif defined(__APPLE__)
    roots = { "/System/Library/Fonts", "/Library/Fonts" };
    if(const char *home = getenv("HOME")) roots.push_back(std::string(home) + "/Library/Fonts");
#else
    roots = { "/usr/share/fonts", "/usr/local/share/fonts" };
    if(const char *home = getenv("HOME")) {
        roots.push_back(std::string(home) + "/.local/share/fonts");
        roots.push_back(std::string(home) + "/.fonts");
    }
#endif
    std::vector<Path> fonts;
    for(const std::string &root : roots) {
        std::error_code ec;
        auto it = std::filesystem::recursive_directory_iterator(
            root, std::filesystem::directory_options::skip_permission_denied, ec);
        for(; !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            std::string ext = it->path().extension().string();
            for(char &c : ext) c = (char)tolower((unsigned char)c);
            if(ext == ".ttf" || ext == ".otf") fonts.push_back(Path::From(it->path().string()));
        }
    }
    return fonts;
}

void OpenInBrowser(const std::string &url) {
    SDL_OpenURL(url.c_str());
}

std::vector<std::string> InitGui(int argc, char **argv) {
#if defined(__EMSCRIPTEN__)
    MountWebStorage(); // before the settings are read
#endif
    std::vector<std::string> args = InitCli(argc, argv);
    // Fingers become mouse events through TouchGestures (pan, pinch, orbit), not SDL's own
    // one-finger mouse.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        FatalError(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    gTimerEvent = SDL_RegisterEvents(1);
    gWakeEvent  = SDL_RegisterEvents(1);
    // Solids are made on a worker thread while dragging; its finish wakes the event loop.
#if !defined(__EMSCRIPTEN__)
    SS.backgroundRegenEnabled = true; // the web build has no threads
#endif
    SS.onRegenFinished        = [] {
        SDL_Event ev = {};
        ev.type      = gWakeEvent;
        SDL_PushEvent(&ev); // (thread-safe)
    };

    int count = 0;
    SDL_Locale **locales = SDL_GetPreferredLocales(&count);
    bool localeSet = false;
    for(int i = 0; locales && i < count && !localeSet; i++) {
        std::string name = locales[i]->language;
        if(locales[i]->country) name += std::string("_") + locales[i]->country;
        localeSet = SetLocale(name);
    }
    SDL_free(locales);
    if(!localeSet) SetLocale("en_US");
    return args;
}

static void LoadKeymap() {
    std::string preset = GetSettings()->ThawString("NG_KeymapPreset", "modern");
    std::string text;
    ReadFile(PrefDir().Join("keymap.txt"), &text);
    std::string error;
    gKeyRouter.SetKeymap(Ui::LoadUserKeymap(preset, text, &error));
    if(!error.empty()) {
        // Show the problem once the main window exists.
        static TimerRef notify = CreateTimer();
        notify->onTimeout = [error]() { Error("%s", error.c_str()); };
        notify->RunAfter(0);
    }
    gKeyRouter.onUiAction = [](const std::string &action) { Ui::HandleUiAction(action); };
    gKeyRouter.modal       = Ui::ModalKey;
    gKeyRouter.modalRepeat = Ui::ModalRepeatKey;
}

static SDL_Window *CreateSdlWindow() {
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
#if defined(__EMSCRIPTEN__)
    flags |= SDL_WINDOW_FILL_DOCUMENT; // the canvas follows the browser window
#endif
    return SDL_CreateWindow("SolveSpace", 1280, 800, flags);
}

static void InitSdlWindow() {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
    // ANGLE on Windows, WebGL 2 in a browser: GLES 3.0 context.
#   if defined(_WIN32)
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    // Our ANGLE build names its DLLs without SDL's expected "lib" prefix.
    SDL_SetHint(SDL_HINT_EGL_LIBRARY, "EGL.dll");
    SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, "GLESv2.dll");
#   endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    // Compatibility context: the renderer uses GLSL 1.20 and no VAOs.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    gSdlWindow = CreateSdlWindow();
    if(!gSdlWindow) {
        // Retry without MSAA; some drivers reject it.
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        gSdlWindow = CreateSdlWindow();
    }
    if(!gSdlWindow) FatalError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    gGlContext = SDL_GL_CreateContext(gSdlWindow);
    if(!gGlContext) FatalError(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    SDL_GL_MakeCurrent(gSdlWindow, gGlContext);
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput(gSdlWindow);
}

static const char *GlslVersion() {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
    return "#version 300 es";
#else
    return "#version 120";
#endif
}

static void FreeIconTextures();

static void ShutdownImGui() {
    FreeIconTextures();
    ImGuiRenderer_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

// The window and GL context are made once; a new main window (the test harness makes one per
// case) gets a fresh ImGui context and fresh NG UI state.
static void InitSdlWindowAndImGui() {
    bool first = gSdlWindow == nullptr;
    if(first) {
        InitSdlWindow();
#if defined(__EMSCRIPTEN__)
        // Every save and export is a download, also a Save to the name the file was opened as.
        SS.OnSaveFinished = [](const Path &path, bool, bool autosave) {
            if(!autosave) gWebDownloads.push_back(path.raw);
        };
#endif
    } else {
        ShutdownImGui();
        gActivePopup = nullptr;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // layout persists through Platform::Settings instead
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetCurrentContext()->TestEngineHookItems = gDriver; // record items for the driver
    ImGui_ImplSDL3_InitForOpenGL(gSdlWindow, gGlContext);
    ImGuiRenderer_Init(GlslVersion());
    gShowMenuBar = GetSettings()->ThawBool("NG_ShowMenuBar", true);
    int windowW = 0, windowH = 0;
    SDL_GetWindowSize(gSdlWindow, &windowW, &windowH);
    Ui::SetCompactLayout(windowW > 0 && windowW < Ui::CompactWidth);
    if(Ui::CompactLayout()) {
        SS.showToolbar = true; // a phone has no keys: the tools at hand
        Ui::SetTouchInput(true);
    }
    Ui::InitNgUi();
    Ui::RegisterCoreUiActions(/*animate=*/true);
    Ui::Actions().Bind("ui.reload-keymap", [] { LoadKeymap(); }); // after a preset change
    Ui::Actions().Bind("ui.toggle-menu-bar", [] {
        gShowMenuBar = !gShowMenuBar;
        GetSettings()->FreezeBool("NG_ShowMenuBar", gShowMenuBar);
    });
    ssassert(Ui::Actions().Unbound().empty(), "every ui.* action must be bound");
    LoadKeymap();
    if(first) {
        LoadDebugScript();
        SDL_ShowWindow(gSdlWindow);
    }
}

static WindowImplSdl *MainWindow() {
    return gMainWindow.lock().get();
}

static bool gTouchPan = false; // a two-finger pan is under way: Shift+middle drag

static MouseEvent MakeMouseEvent(MouseEvent::Type type, double x, double y) {
    MouseEvent me = {};
    me.type = type;
    me.x    = x;
    me.y    = y;
    SDL_Keymod mod = gDriver ? gDriverMods : SDL_GetModState();
    me.shiftDown   = (mod & SDL_KMOD_SHIFT) != 0;
    me.controlDown = (mod & SDL_KMOD_CTRL) != 0;
    if(gTouchPan && type != MouseEvent::Type::SCROLL_VERT) me.shiftDown = true; // not a fine zoom
    return me;
}

static MouseEvent::Button ButtonFromSdl(Uint8 b) {
    switch(b) {
        case SDL_BUTTON_LEFT:   return MouseEvent::Button::LEFT;
        case SDL_BUTTON_MIDDLE: return MouseEvent::Button::MIDDLE;
        case SDL_BUTTON_RIGHT:  return MouseEvent::Button::RIGHT;
        default:                return MouseEvent::Button::NONE;
    }
}

static MouseEvent::Button ButtonFromState(SDL_MouseButtonFlags state) {
    if(state & SDL_BUTTON_LMASK) return MouseEvent::Button::LEFT;
    if(state & SDL_BUTTON_MMASK) return MouseEvent::Button::MIDDLE;
    if(state & SDL_BUTTON_RMASK) return MouseEvent::Button::RIGHT;
    return MouseEvent::Button::NONE;
}

// Whether ImGui takes a mouse event at (x, y). io.WantCaptureMouse is a frame old: the first
// move from a panel onto the canvas (and a click in that same frame) would go nowhere. So ask
// at the event's position, unless ImGui is busy with the mouse (dragging one of its items, a
// popup open) or a drag that started on the canvas is still going on.
static bool ImGuiTakesMouseAt(float x, float y) {
    ImGuiIO      &io = ImGui::GetIO();
    ImGuiContext &g  = *ImGui::GetCurrentContext();
    bool ownedDown = false, canvasDown = false;
    for(int b = 0; b < IM_ARRAYSIZE(io.MouseDown); b++) {
        if(!io.MouseDown[b]) continue;
        if(io.MouseDownOwned[b]) ownedDown = true; else canvasDown = true;
    }
    if(g.ActiveId != 0 || g.OpenPopupStack.Size > 0 || ownedDown) return io.WantCaptureMouse;
    // The view cube takes no ImGui input: it claims its faces, and a press that began on one.
    if(Ui::ViewCubeTakesMouseAt(x, y, canvasDown)) return true;
    if(canvasDown) return false;
    ImGuiWindow *hovered = nullptr, *underMoving = nullptr;
    ImGui::FindHoveredWindowEx(ImVec2(x, y), false, &hovered, &underMoving);
    return hovered != nullptr;
}

static bool gPointerOnCanvas = false;

// Forwards an SDL event to the graphics window unless ImGui owns the mouse/keyboard.
static void ForwardToMainWindow(const SDL_Event &ev) {
    WindowImplSdl *w = MainWindow();
    if(!w || gModalDepth > 0) return;
    switch(ev.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if(ImGuiTakesMouseAt(ev.motion.x, ev.motion.y)) {
                // Onto a panel or menu: the canvas sees the pointer leave, so nothing stays
                // highlighted under it.
                if(gPointerOnCanvas && w->onMouseEvent) {
                    w->onMouseEvent(MakeMouseEvent(MouseEvent::Type::LEAVE, 0, 0));
                }
                gPointerOnCanvas = false;
                return;
            }
            gPointerOnCanvas = true;
            MouseEvent me = MakeMouseEvent(MouseEvent::Type::MOTION, ev.motion.x, ev.motion.y);
            me.button = ButtonFromState(ev.motion.state);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if(ImGuiTakesMouseAt(ev.button.x, ev.button.y)) return;
            MouseEvent::Type t = ev.type == SDL_EVENT_MOUSE_BUTTON_UP ? MouseEvent::Type::RELEASE
                               : ev.button.clicks >= 2 ? MouseEvent::Type::DBL_PRESS
                                                       : MouseEvent::Type::PRESS;
            MouseEvent me = MakeMouseEvent(t, ev.button.x, ev.button.y);
            me.button = ButtonFromSdl(ev.button.button);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            if(ImGuiTakesMouseAt(ev.wheel.mouse_x, ev.wheel.mouse_y)) return;
            MouseEvent me  = MakeMouseEvent(MouseEvent::Type::SCROLL_VERT, ev.wheel.mouse_x,
                                            ev.wheel.mouse_y);
            me.scrollDelta = ev.wheel.y;
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            MouseEvent me = MakeMouseEvent(MouseEvent::Type::LEAVE, 0, 0);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            RouteKeyDown(ev);
            break;
        }
        default: break;
    }
}

static void HandleEvent(const SDL_Event &ev);

static TouchGestures gTouch;

// Sends the mouse events the fingers made through ImGui and on to the canvas, as a mouse's.
static void DispatchTouch() {
    using Out = TouchGestures::Out;
    gTouch.Tick(SDL_GetTicks());
    if(gTouch.Waiting()) gNeedsFrame = true; // to tick again
    static SDL_MouseButtonFlags held = 0;
    // One button change a frame, as a mouse makes them: a tap's press and release, or a long
    // press's, in one frame would reach ImGui's input queue after the menu they open.
    static std::deque<Out> queue;
    for(const Out &o : gTouch.Take()) queue.push_back(o);
    while(!queue.empty()) {
        Out o = queue.front();
        queue.pop_front();
        // Two-finger pan is SolveSpace's Shift+middle drag (MakeMouseEvent adds the Shift).
        Uint8 b = o.button == Out::Button::LEFT     ? SDL_BUTTON_LEFT
                  : o.button == Out::Button::MIDDLE ? SDL_BUTTON_MIDDLE
                  : o.button == Out::Button::PAN    ? SDL_BUTTON_MIDDLE
                  : o.button == Out::Button::RIGHT  ? SDL_BUTTON_RIGHT
                                                    : 0;
        if(o.kind == Out::Kind::PRESS) gTouchPan = o.button == Out::Button::PAN;
        SDL_Event ev = {};
        switch(o.kind) {
            case Out::Kind::MOTION:
                ev.type          = SDL_EVENT_MOUSE_MOTION;
                ev.motion.which  = SDL_TOUCH_MOUSEID;
                ev.motion.x      = (float)o.x;
                ev.motion.y      = (float)o.y;
                ev.motion.state  = held;
                break;
            case Out::Kind::PRESS:
            case Out::Kind::RELEASE:
                if(o.kind == Out::Kind::PRESS) held |= SDL_BUTTON_MASK(b);
                else held &= ~SDL_BUTTON_MASK(b);
                ev.type           = o.kind == Out::Kind::PRESS ? SDL_EVENT_MOUSE_BUTTON_DOWN
                                                               : SDL_EVENT_MOUSE_BUTTON_UP;
                ev.button.which   = SDL_TOUCH_MOUSEID;
                ev.button.button  = b;
                ev.button.down    = o.kind == Out::Kind::PRESS;
                ev.button.clicks  = 1;
                ev.button.x       = (float)o.x;
                ev.button.y       = (float)o.y;
                break;
            case Out::Kind::WHEEL:
                ev.type          = SDL_EVENT_MOUSE_WHEEL;
                ev.wheel.which   = SDL_TOUCH_MOUSEID;
                ev.wheel.y       = (float)o.wheel;
                ev.wheel.mouse_x = (float)o.x;
                ev.wheel.mouse_y = (float)o.y;
                break;
        }
        ev.common.timestamp = SDL_GetTicksNS();
        if(gSdlWindow) ev.motion.windowID = SDL_GetWindowID(gSdlWindow);
        HandleEvent(ev);
        if(o.kind == Out::Kind::RELEASE) gTouchPan = false;
        if(o.kind == Out::Kind::PRESS || o.kind == Out::Kind::RELEASE) {
            if(!queue.empty()) gNeedsFrame = true;
            break;
        }
    }
}

static void HandleFinger(const SDL_Event &ev) {
    int w = 0, h = 0;
    SDL_Window *win = SDL_GetWindowFromID(ev.tfinger.windowID);
    SDL_GetWindowSize(win ? win : gSdlWindow, &w, &h);
    double   x = ev.tfinger.x * w, y = ev.tfinger.y * h;
    uint64_t ms = SDL_NS_TO_MS(ev.tfinger.timestamp);
    switch(ev.type) {
        case SDL_EVENT_FINGER_DOWN:     gTouch.FingerDown(ev.tfinger.fingerID, x, y, ms); break;
        case SDL_EVENT_FINGER_MOTION:   gTouch.FingerMove(ev.tfinger.fingerID, x, y, ms); break;
        case SDL_EVENT_FINGER_UP:       gTouch.FingerUp(ev.tfinger.fingerID, x, y, ms); break;
        case SDL_EVENT_FINGER_CANCELED: gTouch.Cancel(); break;
        default: break;
    }
}

static void HandleEvent(const SDL_Event &ev) {
    gNeedsFrame = true;
    if(ev.type == gWakeEvent) return; // RunGui applies the solid before the frame
    if(ev.type == gTimerEvent) {
        HandleTimerEvent(ev);
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(&ev);
    switch(ev.type) {
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            Ui::SetTouchInput(true);
            HandleFinger(ev); // DispatchTouch() sends its mouse events once a batch
            return;
        case SDL_EVENT_MOUSE_MOTION:
            if(ev.motion.which != SDL_TOUCH_MOUSEID) Ui::SetTouchInput(false);
            ForwardToMainWindow(ev);
            break;
        case SDL_EVENT_KEY_DOWN:
            Ui::SetTouchInput(false);
            ForwardToMainWindow(ev);
            break;
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            gCloseRequested = true; // handled by the top-level loop, never inside a modal
            return;
        default:
            ForwardToMainWindow(ev);
    }
}

static void PumpEventsOnce(bool wait) {
    SDL_Event ev;
#if defined(__EMSCRIPTEN__)
    // The browser delivers events only while we yield to it (ASYNCIFY).
    emscripten_sleep(wait && !gNeedsFrame ? 16 : 0);
    bool got = SDL_PollEvent(&ev);
#else
    bool got = (wait && !gNeedsFrame) ? SDL_WaitEventTimeout(&ev, 500) : SDL_PollEvent(&ev);
#endif
    while(got) {
        HandleEvent(ev);
        got = SDL_PollEvent(&ev);
    }
    DispatchTouch();
#if defined(__EMSCRIPTEN__)
    TakeSoftKeyboardInput();
    DownloadWebFiles();
    SaveWebSettings();
#endif
}

static void DrawEditorOverlay(WindowImplSdl *w, ImVec2 origin) {
    if(!w->editorVisible) return;
    // As the retired GTK frontend did: the core passes the text baseline, so lift the entry
    // by the font ascent (~0.8 em) plus the frame padding.
    ImVec2 pad = ImGui::GetStyle().FramePadding;
    ImGui::SetNextWindowPos(
        ImVec2(origin.x + (float)w->editorX - pad.x,
               origin.y + (float)(w->editorY - 0.8 * w->editorFontHeight) - pad.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##editor", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_AlwaysAutoResize);
    if(w->editorFocusPending) {
        ImGui::SetKeyboardFocusHere();
        w->editorFocusPending = false;
    }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", w->editorText.c_str());
    float textWidth = ImGui::CalcTextSize((w->editorText + "  ").c_str()).x;
    ImGui::SetNextItemWidth(std::max((float)w->editorMinWidth, textWidth) + 2 * pad.x);
    if(ImGui::InputText("##text", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        w->editorVisible = false;
        auto onDone = w->onEditingDone;
        gDeferred.Post([onDone, text = std::string(buf)] { if(onDone) onDone(text); });
    } else {
        w->editorText = buf;
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) w->editorVisible = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

static void DrawTooltip(WindowImplSdl *w, ImVec2 origin) {
    if(w->tooltipText.empty()) return;
    ImVec2 m = ImGui::GetIO().MousePos;
    float  x = m.x - origin.x, y = m.y - origin.y;
    if(x >= w->tooltipX && x <= w->tooltipX + w->tooltipW &&
       y >= w->tooltipY && y <= w->tooltipY + w->tooltipH) {
        ImGui::SetTooltip("%s", w->tooltipText.c_str());
    }
}

#if defined(__APPLE__)
#   define glGenFramebuffers         glGenFramebuffersEXT
#   define glBindFramebuffer         glBindFramebufferEXT
#   define glFramebufferTexture2D    glFramebufferTexture2DEXT
#   define glGenRenderbuffers        glGenRenderbuffersEXT
#   define glBindRenderbuffer        glBindRenderbufferEXT
#   define glRenderbufferStorage     glRenderbufferStorageEXT
#   define glFramebufferRenderbuffer glFramebufferRenderbufferEXT
#   define glDeleteFramebuffers      glDeleteFramebuffersEXT
#   define glDeleteRenderbuffers     glDeleteRenderbuffersEXT
#   define GL_FRAMEBUFFER            GL_FRAMEBUFFER_EXT
#   define GL_RENDERBUFFER           GL_RENDERBUFFER_EXT
#   define GL_COLOR_ATTACHMENT0      GL_COLOR_ATTACHMENT0_EXT
#   define GL_DEPTH_ATTACHMENT       GL_DEPTH_ATTACHMENT_EXT
#endif

static void ReleaseFramebuffer(WindowImplSdl *w) {
    if(!w->fbo || !gGlContext) return;
    glDeleteFramebuffers(1, &w->fbo);
    glDeleteTextures(1, &w->fboColor);
    glDeleteRenderbuffers(1, &w->fboDepth);
    w->fbo = 0;
}

// Icon textures, by "<name>@<px>"; 0 for an icon that doesn't rasterize.
static std::map<std::string, GLuint> gIconTextures;

static void FreeIconTextures() {
    for(auto &[key, tex] : gIconTextures) {
        if(tex && gGlContext) glDeleteTextures(1, &tex);
    }
    gIconTextures.clear();
}

ImTextureID IconTextureFromSvg(const std::string &key, std::string_view svg, int px,
                               const Ui::IconPalette &palette) {
    char colours[64];
    snprintf(colours, sizeof(colours), "/%x/%x/%x/%x/%x/%x", palette.text, palette.entity,
             palette.point, palette.constraint, palette.construction, palette.solid);
    std::string cacheKey = key + "@" + std::to_string(px) + colours;
    auto it = gIconTextures.find(cacheKey);
    if(it != gIconTextures.end()) return (ImTextureID)(intptr_t)it->second;
    GLuint tex = 0;
    if(std::optional<Ui::IconImage> im = Ui::RasterizeIcon(svg, px, palette)) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->width, im->height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, im->rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    gIconTextures[cacheKey] = tex;
    return (ImTextureID)(intptr_t)tex;
}

ImTextureID IconTexture(const std::string &name, int px, const Ui::IconPalette &palette) {
    size_t      size = 0;
    const char *svg  = (const char *)LoadResource("icons/ng/" + name + ".svg", &size);
    return IconTextureFromSvg(name, std::string_view(svg, size), px, palette);
}

static void EnsureFramebuffer(WindowImplSdl *w, int pw, int ph) {
    if(w->fbo && w->fboWidth == pw && w->fboHeight == ph) return;
    ReleaseFramebuffer(w);
    glGenTextures(1, &w->fboColor);
    glBindTexture(GL_TEXTURE_2D, w->fboColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenRenderbuffers(1, &w->fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, w->fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, pw, ph);
    glGenFramebuffers(1, &w->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, w->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, w->fboColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, w->fboDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    w->fboWidth  = pw;
    w->fboHeight = ph;
}

//-----------------------------------------------------------------------------
// Development and test driver. SOLVESPACE_NG_SCRIPT="action ui.command-palette; keys line;
// key Return; wait 5; shot a.png; quit" runs steps between frames, injecting real SDL events.
// Shorthands: SOLVESPACE_NG_SCREENSHOT=<png> [SOLVESPACE_NG_SCREENSHOT_ACTION=<ui.action>].
// (Window-system captures of GL windows are unreliable under XWayland.)
//-----------------------------------------------------------------------------

static std::vector<DebugStep>  gScript;
static size_t                  gScriptPos  = 0;
static int                     gScriptWait = 0;
static std::deque<std::string> gScriptText; // keeps injected TEXT_INPUT strings alive

static void LoadDebugScript() {
    std::string text;
    if(const char *script = getenv("SOLVESPACE_NG_SCRIPT")) {
        text = script;
    } else if(const char *shot = getenv("SOLVESPACE_NG_SCREENSHOT")) {
        if(const char *action = getenv("SOLVESPACE_NG_SCREENSHOT_ACTION")) {
            text += std::string("action ") + action + "; ";
        }
        text += std::string("wait 8; shot ") + shot + "; quit";
    }
    if(text.empty()) return;
    std::string error;
    auto steps = ParseDebugScript(text, &error);
    if(!steps) FatalError("SOLVESPACE_NG_SCRIPT: " + error);
    gScript     = *steps;
    gScriptWait = 2; // let the first layout settle
}

//-----------------------------------------------------------------------------
// Event injection, shared by the debug script and the test driver (uidriver.h). Pushed events
// take the same path as real ones; held keys and buttons are tracked here because SDL does not
// update its own keyboard and mouse state for pushed events.
//-----------------------------------------------------------------------------

static Path gPendingShot;

static SDL_Keymod ModBit(SDL_Keycode key) {
    switch(key) {
        case SDLK_LSHIFT: return SDL_KMOD_LSHIFT;
        case SDLK_RSHIFT: return SDL_KMOD_RSHIFT;
        case SDLK_LCTRL:  return SDL_KMOD_LCTRL;
        case SDLK_RCTRL:  return SDL_KMOD_RCTRL;
        case SDLK_LALT:   return SDL_KMOD_LALT;
        case SDLK_RALT:   return SDL_KMOD_RALT;
        default:          return SDL_KMOD_NONE;
    }
}

static void PushKey(SDL_Keycode key, SDL_Keymod mod, bool down, bool repeat = false) {
    SDL_Event ev    = {};
    ev.type         = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.windowID = SDL_GetWindowID(gSdlWindow);
    ev.key.key      = key;
    ev.key.scancode = SDL_GetScancodeFromKey(key, nullptr);
    ev.key.mod      = mod;
    ev.key.down     = down;
    ev.key.repeat   = repeat;
    SDL_PushEvent(&ev);
}

static void PushText(const std::string &text) {
    gScriptText.push_back(text);
    SDL_Event ev     = {};
    ev.type          = SDL_EVENT_TEXT_INPUT;
    ev.text.windowID = SDL_GetWindowID(gSdlWindow);
    ev.text.text     = gScriptText.back().c_str();
    SDL_PushEvent(&ev);
}

// "Ctrl+Shift+Keypad 1": SDL key names with modifier prefixes; "Shift", "Ctrl" and "Alt" alone
// name the left modifier keys. False for an unknown key name.
static bool ParseKeyChord(std::string name, SDL_Keycode *key, SDL_Keymod *mods) {
    *mods = SDL_KMOD_NONE;
    static const std::pair<const char *, SDL_Keymod> prefixes[] = {
        { "Ctrl+", SDL_KMOD_LCTRL }, { "Shift+", SDL_KMOD_LSHIFT }, { "Alt+", SDL_KMOD_LALT } };
    for(bool again = true; again;) {
        again = false;
        for(auto [prefix, bit] : prefixes) {
            size_t n = strlen(prefix);
            if(name.size() > n && name.compare(0, n, prefix) == 0) {
                *mods = (SDL_Keymod)(*mods | bit);
                name  = name.substr(n);
                again = true;
            }
        }
    }
    *key = name == "Shift" ? SDLK_LSHIFT
         : name == "Ctrl"  ? SDLK_LCTRL
         : name == "Alt"   ? SDLK_LALT
                           : SDL_GetKeyFromName(name.c_str());
    return *key != SDLK_UNKNOWN;
}

// The text a key types with these modifiers on the current layout, or "" for none.
static std::string TextForKey(SDL_Keycode key, SDL_Keymod mods) {
    if(mods & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) return "";
    SDL_Scancode sc = SDL_GetScancodeFromKey(key, nullptr);
    SDL_Keycode produced =
        SDL_GetKeyFromScancode(sc, (SDL_Keymod)(mods & SDL_KMOD_SHIFT), /*key_event=*/false);
    if(produced < 32 || produced > 126) return "";
    return std::string(1, (char)produced);
}

static const SDL_Keycode ModifierKeys[] = { SDLK_LCTRL, SDLK_LSHIFT, SDLK_LALT };

// Presses the chord's modifiers that are not already held, then the key (with its text).
// Returns the modifiers it pressed, for ReleaseChord.
static SDL_Keymod PressChord(SDL_Keycode key, SDL_Keymod mods, bool withText) {
    SDL_Keymod pressed = SDL_KMOD_NONE;
    for(SDL_Keycode m : ModifierKeys) {
        if((mods & ModBit(m)) && !(gDriverMods & ModBit(m))) {
            gDriverMods = (SDL_Keymod)(gDriverMods | ModBit(m));
            pressed     = (SDL_Keymod)(pressed | ModBit(m));
            PushKey(m, gDriverMods, true);
        }
    }
    if(SDL_Keymod bit = ModBit(key)) {
        gDriverMods = (SDL_Keymod)(gDriverMods | bit);
        PushKey(key, gDriverMods, true);
        return (SDL_Keymod)(pressed | bit);
    }
    PushKey(key, gDriverMods, true);
    if(withText) {
        std::string text = TextForKey(key, gDriverMods);
        if(!text.empty()) PushText(text);
    }
    return pressed;
}

static void ReleaseChord(SDL_Keycode key, SDL_Keymod pressed) {
    if(!ModBit(key)) PushKey(key, gDriverMods, false);
    for(int i = (int)(sizeof(ModifierKeys) / sizeof(ModifierKeys[0])) - 1; i >= 0; i--) {
        SDL_Keycode m = ModifierKeys[i];
        if(key == m || ((pressed & ModBit(m)) && (gDriverMods & ModBit(m)))) {
            gDriverMods = (SDL_Keymod)(gDriverMods & ~ModBit(m));
            PushKey(m, gDriverMods, false);
        }
    }
}

// Types one character: its key (with Shift if the layout needs it), then the text.
static void TypeChar(char c) {
    SDL_Keymod   need = SDL_KMOD_NONE;
    SDL_Scancode sc   = SDL_GetScancodeFromKey((SDL_Keycode)(unsigned char)c, &need);
    if(sc == SDL_SCANCODE_UNKNOWN && isupper((unsigned char)c)) {
        sc   = SDL_GetScancodeFromKey((SDL_Keycode)tolower((unsigned char)c), nullptr);
        need = SDL_KMOD_SHIFT;
    }
    if(sc == SDL_SCANCODE_UNKNOWN) {
        PushText(std::string(1, c));
        return;
    }
    SDL_Keycode base = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, /*key_event=*/false);
    SDL_Keymod  pressed =
        PressChord(base, (need & SDL_KMOD_SHIFT) ? SDL_KMOD_LSHIFT : SDL_KMOD_NONE, false);
    PushText(std::string(1, c));
    ReleaseChord(base, pressed);
}

static void PushMotion(float x, float y) {
    SDL_Event ev       = {};
    ev.type            = SDL_EVENT_MOUSE_MOTION;
    ev.motion.windowID = SDL_GetWindowID(gSdlWindow);
    ev.motion.x        = x;
    ev.motion.y        = y;
    ev.motion.xrel     = x - gDriverX;
    ev.motion.yrel     = y - gDriverY;
    ev.motion.state    = gDriverButtons;
    gDriverX = x;
    gDriverY = y;
    SDL_PushEvent(&ev);
}

static void PushButton(Uint8 button, bool down, int clicks) {
    if(down) {
        gDriverButtons |= SDL_BUTTON_MASK(button);
    } else {
        gDriverButtons &= ~SDL_BUTTON_MASK(button);
    }
    SDL_Event ev       = {};
    ev.type            = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.windowID = SDL_GetWindowID(gSdlWindow);
    ev.button.button   = button;
    ev.button.down     = down;
    ev.button.clicks   = (Uint8)clicks;
    ev.button.x        = gDriverX;
    ev.button.y        = gDriverY;
    SDL_PushEvent(&ev);
}

static void SaveBackBuffer(const Path &path, int pw, int ph) {
    std::shared_ptr<Pixmap> pm = Pixmap::Create(Pixmap::Format::RGBA, pw, ph);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, &pm->data[0]);
    pm->WritePng(path, /*flip=*/true);
}

static void RunDebugScript(int pw, int ph) {
    if(gScriptPos >= gScript.size()) return;
    gNeedsFrame = true;
    // Typed text reaches ImGui a character or so per frame: finish it before the next step,
    // or "keys toggle theme; key Return" presses Return after "toggl".
    if(ImGui::GetCurrentContext()->InputEventsQueue.Size > 0 ||
       SDL_HasEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST)) {
        return;
    }
    if(gScriptWait > 0) {
        gScriptWait--;
        return;
    }
    while(gScriptPos < gScript.size()) {
        const DebugStep &step = gScript[gScriptPos++];
        switch(step.kind) {
            case DebugStep::Kind::ACTION:
                Ui::HandleUiAction(step.arg);
                break;
            case DebugStep::Kind::KEYS:
                for(char c : step.arg) TypeChar(c);
                break;
            case DebugStep::Kind::KEY: {
                SDL_Keycode key;
                SDL_Keymod  mods;
                if(!ParseKeyChord(step.arg, &key, &mods)) {
                    FatalError("SOLVESPACE_NG_SCRIPT: unknown key " + step.arg);
                }
                ReleaseChord(key, PressChord(key, mods, /*withText=*/true));
                break;
            }
            case DebugStep::Kind::WAIT:
                gScriptWait = step.frames;
                return;
            case DebugStep::Kind::MOVE:
                PushMotion(step.x, step.y);
                break;
            case DebugStep::Kind::CLICK:
                PushMotion(step.x, step.y);
                PushButton(SDL_BUTTON_LEFT, true, 1);
                PushButton(SDL_BUTTON_LEFT, false, 1);
                break;
            case DebugStep::Kind::SHOT:
                SaveBackBuffer(Path::From(step.arg), pw, ph);
                break;
            case DebugStep::Kind::QUIT:
                gNeedsFrame = false;
                ExitGui();
                return;
        }
    }
}

#if defined(__EMSCRIPTEN__)
// A phone's keyboard comes up only for a focused HTML text field: ngshell.html keeps a hidden
// one, focused while an ImGui field wants text after a touch, and queues what is typed in
// Module.ngTyped (code points; -1 Backspace, -2 Enter, -3 Esc, -4 Tab). The page can't call in
// while the app sleeps (ASYNCIFY), so the event loop takes them from there.
static void TakeSoftKeyboardInput() {
    ImGuiIO &io = ImGui::GetIO();
    for(;;) {
        int c = EM_ASM_INT({ var q = Module.ngTyped; return q && q.length ? q.shift() : 0; });
        if(c == 0) break;
        gNeedsFrame = true;
        if(c > 0) {
            io.AddInputCharacter((unsigned)c);
            continue;
        }
        ImGuiKey k = c == -1 ? ImGuiKey_Backspace : c == -2 ? ImGuiKey_Enter
                   : c == -3 ? ImGuiKey_Escape    : ImGuiKey_Tab;
        io.AddKeyEvent(k, true);
        io.AddKeyEvent(k, false);
    }
}

static void ShowSoftKeyboard() {
    static bool shown = false;
    bool        want  = ImGui::GetIO().WantTextInput && Ui::TouchInput();
    if(want == shown) return;
    shown = want;
    EM_ASM({ if(Module.ngSoftKeyboard) Module.ngSoftKeyboard($0); }, want);
}
#endif

static void RenderFrame() {
    if(!gSdlWindow) return;
    gNeedsFrame = false;

    ImGuiRenderer_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    WindowImplSdl *main = MainWindow();
    DrawMainMenuBar();
    DrawActivePopup();
    if(main && main->menuBar) {
        auto bar = std::static_pointer_cast<MenuBarSdl>(main->menuBar);
        if(auto run = Ui::DrawNgUi(bar.get(), gKeyRouter.Keymap())) gDeferred.Post(run);
    }
    if(main) {
        DrawEditorOverlay(main, ImVec2(0, 0));
        DrawTooltip(main, ImVec2(0, 0));
        if(!ImGui::GetIO().WantCaptureMouse) {
            ImGui::SetMouseCursor(main->cursor == Window::Cursor::HAND ? ImGuiMouseCursor_Hand
                                                                       : ImGuiMouseCursor_Arrow);
        }
    }
    ImGui::Render();
#if defined(__EMSCRIPTEN__)
    ShowSoftKeyboard();
#endif

    int pw, ph;
    SDL_GetWindowSizeInPixels(gSdlWindow, &pw, &ph);
    if(main && main->visible && main->onRender) {
        main->onRender(); // the renderer sets its own viewport
    } else {
        glViewport(0, 0, pw, ph);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glViewport(0, 0, pw, ph);
    ImGuiRenderer_RenderDrawData(ImGui::GetDrawData());
    RunDebugScript(pw, ph);
    if(!gPendingShot.IsEmpty()) {
        SaveBackBuffer(gPendingShot, pw, ph);
        gPendingShot = {};
    }
    SDL_GL_SwapWindow(gSdlWindow);

    // What ran (a menu item: a panel shown, the menu closing) shows on the next frame, which
    // nothing else asks for when no mouse moves after a tap.
    if(!gDeferred.Empty()) gNeedsFrame = true;
    gDeferred.RunAll();
    // ImGui spreads a burst of input (fast typing) over several frames; render until it's done.
    if(ImGui::GetCurrentContext()->InputEventsQueue.Size > 0) gNeedsFrame = true;
    // A window that sizes itself to its contents (the tool shelf) is hidden while it measures
    // them and takes its size on a later frame; without more frames it would stay blank or
    // clipped until the next input.
    // (A few frames at most: a size that wobbles by rounding must not keep the app busy.)
    static int fitFrames = 0;
    bool       fitting   = false;
    for(ImGuiWindow *w : ImGui::GetCurrentContext()->Windows) {
        if(!w->Active) continue;
        ImVec2 next = ImGui::CalcWindowNextAutoFitSize(w);
        bool   grows = (w->Flags & ImGuiWindowFlags_AlwaysAutoResize) &&
                       (fabsf(w->Size.x - next.x) > 1 || fabsf(w->Size.y - next.y) > 1);
        if(w->Hidden || grows) fitting = true;
    }
    fitFrames = fitting ? fitFrames + 1 : 0;
    if(fitting && fitFrames <= 4) gNeedsFrame = true;
}

void RunGui() {
    while(!gQuit) {
        PumpEventsOnce(/*wait=*/true);
        if(SS.PollRegeneration()) gNeedsFrame = true;
        if(gNeedsFrame) RenderFrame();
        if(gCloseRequested && gModalDepth == 0) {
            gCloseRequested = false;
            WindowImplSdl *w = MainWindow();
            if(w && w->onClose) w->onClose(); else gQuit = true;
        }
    }
}

void ExitGui() {
    gQuit = true;
}

void ClearGui() {
    if(gSdlWindow) {
        // Free GL objects while their context still exists: the canvases would otherwise be
        // destroyed with SS, after main returns, and call glDelete* with no context.
        SDL_GL_MakeCurrent(gSdlWindow, gGlContext);
        SS.GW.persistentCanvas.reset();
        SS.GW.canvas.reset();
        SS.TW.canvas.reset();
        if(WindowImplSdl *main = MainWindow()) ReleaseFramebuffer(main);
        ShutdownImGui();
        SDL_GL_DestroyContext(gGlContext);
        SDL_DestroyWindow(gSdlWindow);
        gGlContext = nullptr;
        gSdlWindow = nullptr;
    }
    std::static_pointer_cast<SettingsFile>(GetSettings())->Save();
    SDL_Quit();
}

// Returns true if an item was activated (so popups can close).
static bool DrawMenuEntries(const MenuSdl &menu) {
    bool activated = false;
    for(const MenuSdl::Entry &e : menu.entries) {
        switch(e.kind) {
            case MenuSdl::Entry::Kind::SEPARATOR:
                ImGui::Separator();
                break;
            case MenuSdl::Entry::Kind::SUBMENU:
                if(ImGui::BeginMenu(e.label.c_str())) {
                    activated |= DrawMenuEntries(*e.submenu);
                    ImGui::EndMenu();
                }
                break;
            case MenuSdl::Entry::Kind::ITEM: {
                const MenuItemSdl &item = *e.item;
                std::string shortcut = Ui::ShortcutText(item, gKeyRouter.Keymap());
                bool checked = item.indicator != MenuItem::Indicator::NONE &&
                               (item.isActive ? item.isActive() : item.active);
                if(ImGui::MenuItem(item.label.c_str(),
                                   shortcut.empty() ? nullptr : shortcut.c_str(), checked,
                                   item.enabled)) {
                    // Keep the item alive: running it may rebuild this very menu.
                    std::shared_ptr<MenuItemSdl> chosen = e.item;
                    gDeferred.Post([chosen] { if(chosen->onTrigger) chosen->onTrigger(); });
                    activated = true;
                }
                break;
            }
        }
    }
    return activated;
}

static void DrawMainMenuBar() {
    WindowImplSdl *main = MainWindow();
    if(!gShowMenuBar || !main || !main->menuBar) return;
    auto bar = std::static_pointer_cast<MenuBarSdl>(main->menuBar);
    if(ImGui::BeginMainMenuBar()) {
        // What doesn't fit (on a phone) goes in a More menu at the end.
        float              gap = 2 * ImGui::GetStyle().ItemSpacing.x; // as BeginMenu spaces them
        std::vector<float> widths;
        for(auto &m : bar->menus) widths.push_back(ImGui::CalcTextSize(m.first.c_str()).x + gap);
        const char *more   = "More";
        size_t      fit    = Ui::MenusThatFit(widths, ImGui::GetContentRegionAvail().x,
                                              ImGui::CalcTextSize(more).x + gap);
        for(size_t i = 0; i < fit; i++) {
            auto &m = bar->menus[i];
            if(ImGui::BeginMenu(m.first.c_str())) {
                DrawMenuEntries(*m.second);
                ImGui::EndMenu();
            }
        }
        if(fit < bar->menus.size() && ImGui::BeginMenu((std::string(more) + "###more").c_str())) {
            for(size_t i = fit; i < bar->menus.size(); i++) {
                auto &m = bar->menus[i];
                if(ImGui::BeginMenu(m.first.c_str())) {
                    DrawMenuEntries(*m.second);
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

static void DrawActivePopup() {
    if(!gActivePopup) return;
    if(!gPopupOpened) {
        ImGui::OpenPopup("##context");
        gPopupOpened = true;
    }
    if(ImGui::BeginPopup("##context")) {
        if(DrawMenuEntries(*gActivePopup)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    } else {
        gActivePopup = nullptr; // dismissed (click outside / Esc) or item chosen
    }
}

// True if a text field is or is about to be focused. ImGui only reports WantTextInput a frame
// after a field takes focus, and keys typed right after opening one arrive before that frame.
static bool TextEntryActive() {
    if(ImGui::GetIO().WantTextInput || Ui::CapturesText()) return true;
    if(WindowImplSdl *main = MainWindow()) {
        if(main->editorVisible) return true;
    }
    return false;
}

static bool RouteKeyDown(const SDL_Event &ev) {
    SDL_Keymod  mod  = ev.key.mod;
    SDL_Keycode base = SDL_GetKeyFromScancode(ev.key.scancode, SDL_KMOD_NONE, false);
    // Right Alt may be AltGr: look the key up on the AltGr level (MODE) too.
    SDL_Keymod layer = (mod & SDL_KMOD_RALT) ? (SDL_Keymod)(mod | SDL_KMOD_MODE) : mod;
    SDL_Keycode produced = SDL_GetKeyFromScancode(ev.key.scancode, layer, false);
    auto chord = ChordFromKey(base, produced, mod);
    if(!chord) return false;
    // An open menu (bar dropdown, context menu, combo) takes the keys as a text field does:
    // ImGui moves through it and Esc closes it; only Ctrl chords and F-keys run commands.
    bool menuOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId |
                                               ImGuiPopupFlags_AnyPopupLevel);
    bool text     = TextEntryActive();
    bool captured = text || menuOpen;
    bool inMenu   = menuOpen && !text;
    Ui::RouteResult r = ev.key.repeat ? gKeyRouter.RouteRepeat(*chord, captured, inMenu)
                                      : gKeyRouter.Route(*chord, captured, inMenu);
    return r != Ui::RouteResult::UNBOUND;
}

//-----------------------------------------------------------------------------
// Test driver (uidriver.h)
//-----------------------------------------------------------------------------

namespace Driver {

void Enable(const Path &configDir) {
    gDriver    = true;
    gConfigDir = configDir;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "offscreen", SDL_HINT_DEFAULT);
}

bool Enabled() {
    return gDriver;
}

static Uint8 SdlButton(Button b) {
    switch(b) {
        case Button::LEFT:   return SDL_BUTTON_LEFT;
        case Button::MIDDLE: return SDL_BUTTON_MIDDLE;
        case Button::RIGHT:  return SDL_BUTTON_RIGHT;
    }
    return SDL_BUTTON_LEFT;
}

void BeginCase() {
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    gDialogs.clear();
    gMessagePolicy = nullptr;
    gQueuedFiles.clear();
    gModalSteps.clear();
    gScriptText.clear();
    gSettleTimeouts = 0;
    gDriverMods    = SDL_KMOD_NONE;
    gDriverButtons = 0;
    gSettings.reset(); // first: it saves itself when destroyed
    std::error_code ec;
    std::filesystem::remove(PrefDir().Join("settings.txt").raw, ec);
    std::filesystem::remove(PrefDir().Join("keymap.txt").raw, ec);
}

std::vector<std::string> EndCase() {
    std::vector<std::string> problems;
    for(const DialogRecord &d : gDialogs) {
        if(d.kind == "unanswered-modal") problems.push_back("a context menu had no queued step");
    }
    if(!gQueuedFiles.empty()) problems.push_back("queued file-dialog answers were not used");
    if(!gModalSteps.empty()) problems.push_back("queued modal steps were not used");
    if(gSettleTimeouts > 0) problems.push_back("the app never went idle (a busy loop?)");
    return problems;
}

void Frame(int n) {
    for(int i = 0; i < n; i++) {
        PumpEventsOnce(/*wait=*/false);
        RenderFrame();
    }
}

void Settle(unsigned horizonMs) {
    uint64_t    start = SDL_GetTicks();
    const char *busy  = "";
    for(int frames = 0;;) {
        PumpEventsOnce(/*wait=*/false);
        if(SS.PollRegeneration()) gNeedsFrame = true;
        if(frames < 3 || gNeedsFrame || !gDeferred.Empty()) {
            busy = gNeedsFrame ? "a frame was requested" : "deferred work";
            RenderFrame();
            frames++;
        } else if(SS.RegenerationPending()) {
            busy = "a solid is being made on the worker";
            SDL_Delay(1);
        } else if(!SDL_HasEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST)) {
            uint64_t due = Timers().EarliestDue(), now = SDL_GetTicks();
            if(due > now + horizonMs) break;
            busy = "a timer is due";
            SDL_Delay((Uint32)std::min<uint64_t>(due > now ? due - now : 0, 5));
        }
        if(SDL_GetTicks() - start > 20000) {
            fprintf(stderr, "Driver::Settle: the app is still busy after 20 s (%s)\n", busy);
            gSettleTimeouts++;
            break;
        }
    }
}

void Key(const std::string &chord) {
    SDL_Keycode key;
    SDL_Keymod  mods;
    if(!ParseKeyChord(chord, &key, &mods)) ssassert(false, "Driver::Key: unknown key name");
    ReleaseChord(key, PressChord(key, mods, /*withText=*/true));
    Settle();
}

void KeyDown(const std::string &chord) {
    SDL_Keycode key;
    SDL_Keymod  mods;
    if(!ParseKeyChord(chord, &key, &mods)) ssassert(false, "Driver::KeyDown: unknown key name");
    PressChord(key, mods, /*withText=*/true);
    Settle();
}

void KeyUp(const std::string &chord) {
    SDL_Keycode key;
    SDL_Keymod  mods;
    if(!ParseKeyChord(chord, &key, &mods)) ssassert(false, "Driver::KeyUp: unknown key name");
    ReleaseChord(key, mods);
    Settle();
}

void KeyRepeat(const std::string &chord, int repeats) {
    SDL_Keycode key;
    SDL_Keymod  mods;
    if(!ParseKeyChord(chord, &key, &mods)) ssassert(false, "Driver::KeyRepeat: unknown key name");
    SDL_Keymod pressed = PressChord(key, mods, /*withText=*/true);
    Settle();
    for(int i = 0; i < repeats; i++) {
        PushKey(key, gDriverMods, true, /*repeat=*/true);
        std::string text = TextForKey(key, gDriverMods);
        if(!text.empty()) PushText(text);
        Settle();
    }
    ReleaseChord(key, pressed);
    Settle();
}

void Type(const std::string &text) {
    for(char c : text) TypeChar(c);
    Settle();
}

void Move(double x, double y) {
    PushMotion((float)x, (float)y);
    Settle();
}

void Press(Button b) {
    PushButton(SdlButton(b), true, 1);
    Settle();
}

void Release(Button b) {
    PushButton(SdlButton(b), false, 1);
    Settle();
}

void Click(double x, double y, Button b) {
    Move(x, y);
    Press(b);
    Release(b);
}

void DoubleClick(double x, double y) {
    // One button change per frame, as with a real mouse: the core acts on each press at once
    // (the second opens a dimension's editor), while ImGui sees it on the next frame.
    Move(x, y);
    for(int clicks = 1; clicks <= 2; clicks++) {
        PushButton(SDL_BUTTON_LEFT, true, clicks);
        Frame(1);
        PushButton(SDL_BUTTON_LEFT, false, clicks);
        Frame(1);
    }
    Settle();
}

void Drag(double x0, double y0, double x1, double y1, Button b, int steps) {
    Move(x0, y0);
    Press(b);
    for(int i = 1; i <= steps; i++) {
        double t = (double)i / steps;
        Move(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
    }
    Release(b);
}

void Wheel(double x, double y, double dy) {
    Move(x, y);
    SDL_Event ev      = {};
    ev.type           = SDL_EVENT_MOUSE_WHEEL;
    ev.wheel.windowID = SDL_GetWindowID(gSdlWindow);
    ev.wheel.y        = (float)dy;
    ev.wheel.mouse_x  = gDriverX;
    ev.wheel.mouse_y  = gDriverY;
    ev.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    SDL_PushEvent(&ev);
    Settle();
}

static void PushFinger(Uint32 type, int finger, Point2d p) {
    int w = 0, h = 0;
    SDL_GetWindowSize(gSdlWindow, &w, &h);
    SDL_Event ev        = {};
    ev.type             = type;
    ev.tfinger.touchID  = 1;
    ev.tfinger.fingerID = (SDL_FingerID)finger + 1;
    ev.tfinger.x        = (float)(p.x / w);
    ev.tfinger.y        = (float)(p.y / h);
    ev.tfinger.pressure = type == SDL_EVENT_FINGER_UP ? 0.0f : 1.0f;
    ev.tfinger.windowID = SDL_GetWindowID(gSdlWindow);
    SDL_PushEvent(&ev);
}

void Tap(double x, double y) {
    PushFinger(SDL_EVENT_FINGER_DOWN, 0, Point2d::From(x, y));
    PushFinger(SDL_EVENT_FINGER_UP, 0, Point2d::From(x, y));
    Settle();
}

void LongPress(double x, double y) {
    PushFinger(SDL_EVENT_FINGER_DOWN, 0, Point2d::From(x, y));
    Uint64 until = SDL_GetTicks() + TouchGestures::LONG_PRESS_MS + 50;
    while(SDL_GetTicks() < until) {
        Frame(1);
        SDL_Delay(10);
    }
    PushFinger(SDL_EVENT_FINGER_UP, 0, Point2d::From(x, y));
    Settle();
}

void TouchDrag(const std::vector<Point2d> &from, const std::vector<Point2d> &to, int steps) {
    for(size_t i = 0; i < from.size(); i++) PushFinger(SDL_EVENT_FINGER_DOWN, (int)i, from[i]);
    Frame(1);
    for(int k = 1; k <= steps; k++) {
        double t = (double)k / steps;
        for(size_t i = 0; i < from.size(); i++) {
            Point2d p = from[i].Plus(to[i].Minus(from[i]).ScaledBy(t));
            PushFinger(SDL_EVENT_FINGER_MOTION, (int)i, p);
        }
        Frame(1);
    }
    for(size_t i = 0; i < from.size(); i++) PushFinger(SDL_EVENT_FINGER_UP, (int)i, to[i]);
    Settle();
}

Point2d MousePos() {
    return Point2d::From(gDriverX, gDriverY);
}

Point2d WindowSize() {
    int w, h;
    SDL_GetWindowSize(gSdlWindow, &w, &h);
    return Point2d::From(w, h);
}

void Resize(int width, int height) {
    SDL_SetWindowSize(gSdlWindow, width, height);
    SDL_SyncWindow(gSdlWindow);
    Settle();
}

Point2d ToWindow(Vector modelPoint) {
    // The inverse of GraphicsWindow::MouseEvent: origin at the centre, y up.
    double w = 0, h = 0;
    if(WindowImplSdl *main = MainWindow()) main->GetContentSize(&w, &h);
    Point2d p = SS.GW.ProjectPoint(modelPoint);
    return Point2d::From(p.x + w / 2, h / 2 - p.y);
}

std::vector<Item> Items() {
    std::vector<Item> out;
    for(const UiItem &u : LastFrameItems()) {
        if(!u.visible) continue; // a person can't see or click it either
        out.push_back({ u.label, u.id, u.window, u.x0, u.y0, u.x1, u.y1, u.checkable,
                        u.checked, u.opened, u.openable, u.isWindow, u.fullyVisible, u.vx0,
                        u.vy0, u.vx1, u.vy1 });
    }
    return out;
}

std::optional<Item> FindItem(const std::string &label) {
    std::vector<Item>   items = Items();
    std::optional<Item> byId, window;
    for(auto it = items.rbegin(); it != items.rend(); ++it) {
        if(it->label == label) {
            if(!it->isWindow) return *it;
            if(!window) window = *it;
        } else if(!byId && !it->isWindow && !label.empty() && it->id == label) {
            byId = *it;
        }
    }
    return byId ? byId : window;
}

bool ClickItem(const std::string &label) {
    std::optional<Item> item = FindItem(label);
    if(!item) return false;
    Point2d c = item->Center();
    Click(c.x, c.y);
    return true;
}

bool ClickMenu(const std::vector<std::string> &path) {
    for(const std::string &label : path) {
        if(!ClickItem(label)) return false;
    }
    return true;
}

bool Screenshot(const Path &png) {
    gPendingShot = png;
    gNeedsFrame  = true;
    Frame(1);
    FILE *f = OpenFile(png, "rb");
    if(f) fclose(f);
    return f != nullptr;
}

void ReloadKeymap() {
    LoadKeymap();
    Settle();
}

Path ConfigDir() {
    return PrefDir();
}

std::vector<DialogRecord> &Dialogs() {
    return gDialogs;
}

void OnMessage(std::function<MessageDialog::Response(const DialogRecord &)> policy) {
    gMessagePolicy = std::move(policy);
}

void QueueFile(const Path &path) {
    gQueuedFiles.push_back(path);
}

void DuringModal(std::function<void()> step) {
    gModalSteps.push_back(std::move(step));
}

}

}
}
