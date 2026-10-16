//-----------------------------------------------------------------------------
// The SDL3 + Dear ImGui platform backend (solvespace-ng).
//
// One SDL window, one GL context. The graphics window draws into the default framebuffer;
// TOOL windows (the property browser) draw into offscreen framebuffers shown as ImGui panels.
//-----------------------------------------------------------------------------
#include <atomic>
#include <filesystem>
#include <mutex>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "solvespace.h"
#include "render/gl3shader.h"
#include "platform/sdl/settingsfile.h"
#include "platform/sdl/menumodel.h"
#include "platform/sdl/timerregistry.h"
#include "platform/sdl/dialoghelpers.h"

namespace SolveSpace {
namespace Platform {

class WindowImplSdl;

static SDL_Window    *gSdlWindow   = nullptr;
static SDL_GLContext  gGlContext   = nullptr;
static Uint32         gTimerEvent  = 0;
static bool           gQuit        = false;
static bool           gNeedsFrame  = true;
static std::weak_ptr<WindowImplSdl>              gMainWindow;
static std::vector<std::weak_ptr<WindowImplSdl>> gToolWindows;

static void DrawToolWindows();
static void DrawMainMenuBar();
static bool RouteKeyDown(const SDL_Event &e);
static void RenderFrame();

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SolveSpace: fatal error",
                             message.c_str(), gSdlWindow);
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

static Path PrefDir() {
    char *pref = SDL_GetPrefPath("solvespace", "solvespace-ng");
    Path dir = Path::From(pref ? pref : ".");
    SDL_free(pref);
    return dir;
}

SettingsRef GetSettings() {
    static std::shared_ptr<SettingsFile> settings;
    if(!settings) settings = std::make_shared<SettingsFile>(PrefDir().Join("settings.txt"));
    return settings;
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
        serial = Timers().Arm(weak_from_this());
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

static std::shared_ptr<MenuBarSdl> gMainMenu;

MenuRef CreateMenu() {
    return std::make_shared<MenuSdl>();
}

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    if(!gMainMenu) gMainMenu = std::make_shared<MenuBarSdl>();
    return gMainMenu;
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

    explicit WindowImplSdl(Kind kind) : kind(kind) {}

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

    void SetMenuBar(MenuBarRef mb) override { menuBar = mb; }

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
        window->width  = 420;
        window->height = 640;
        gToolWindows.push_back(window);
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
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplSdl>();
}

//-----------------------------------------------------------------------------
// Nested event loop (used by blocking file dialogs and context menus)
//-----------------------------------------------------------------------------

static void PumpEventsOnce(bool wait);

static void RunNestedLoop(const std::function<bool()> &done) {
    while(!done() && !gQuit) {
        PumpEventsOnce(/*wait=*/true);
        RenderFrame();
    }
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplSdl final : public FileDialog {
public:
    bool                    isSave;
    std::string             title;
    Path                    filename;
    std::vector<FileFilter> filters;

    explicit FileDialogImplSdl(bool isSave) : isSave(isSave) {}

    void SetTitle(std::string t) override { title = t; }
    void SetCurrentName(std::string name) override { filename = filename.Parent().Join(name); }
    Path GetFilename() override { return filename; }
    void SetFilename(Path path) override { filename = path; }
    void SuggestFilename(Path path) override { filename = path; }
    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        filters.push_back({ name, extensions });
    }
    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        settings->FreezeString("Dialog_" + key + "_Folder", filename.Parent().raw);
    }
    void ThawChoices(SettingsRef settings, const std::string &key) override {
        std::string folder = settings->ThawString("Dialog_" + key + "_Folder", "");
        if(!folder.empty() && filename.IsEmpty()) filename = Path::From(folder).Join("");
    }

    bool RunModal() override {
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
    std::vector<std::string> args = InitCli(argc, argv);
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        FatalError(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    gTimerEvent = SDL_RegisterEvents(1);

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

static SDL_Window *CreateSdlWindow() {
    return SDL_CreateWindow("SolveSpace", 1280, 800,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
}

static void InitSdlWindowAndImGui() {
#if defined(_WIN32)
    // ANGLE: GLES 3.0 context.
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    const char *glslVersion = "#version 300 es";
#else
    // Compatibility context: the renderer uses GLSL 1.20 and no VAOs.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    const char *glslVersion = "#version 120";
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // layout persists through Platform::Settings instead
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL3_InitForOpenGL(gSdlWindow, gGlContext);
    ImGui_ImplOpenGL3_Init(glslVersion);
    SDL_ShowWindow(gSdlWindow);
}

static WindowImplSdl *MainWindow() {
    return gMainWindow.lock().get();
}

static MouseEvent MakeMouseEvent(MouseEvent::Type type, double x, double y) {
    MouseEvent me = {};
    me.type = type;
    me.x    = x;
    me.y    = y;
    SDL_Keymod mod = SDL_GetModState();
    me.shiftDown   = (mod & SDL_KMOD_SHIFT) != 0;
    me.controlDown = (mod & SDL_KMOD_CTRL) != 0;
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

// Forwards an SDL event to the graphics window unless ImGui owns the mouse/keyboard.
static void ForwardToMainWindow(const SDL_Event &ev) {
    WindowImplSdl *w = MainWindow();
    if(!w) return;
    ImGuiIO &io = ImGui::GetIO();
    switch(ev.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if(io.WantCaptureMouse) return;
            MouseEvent me = MakeMouseEvent(MouseEvent::Type::MOTION, ev.motion.x, ev.motion.y);
            me.button = ButtonFromState(ev.motion.state);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if(io.WantCaptureMouse) return;
            MouseEvent::Type t = ev.type == SDL_EVENT_MOUSE_BUTTON_UP ? MouseEvent::Type::RELEASE
                               : ev.button.clicks >= 2 ? MouseEvent::Type::DBL_PRESS
                                                       : MouseEvent::Type::PRESS;
            MouseEvent me = MakeMouseEvent(t, ev.button.x, ev.button.y);
            me.button = ButtonFromSdl(ev.button.button);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            if(io.WantCaptureMouse) return;
            float mx, my;
            SDL_GetMouseState(&mx, &my);
            MouseEvent me  = MakeMouseEvent(MouseEvent::Type::SCROLL_VERT, mx, my);
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
            if(io.WantCaptureKeyboard) return;
            RouteKeyDown(ev);
            break;
        }
        default: break;
    }
}

static void HandleEvent(const SDL_Event &ev) {
    gNeedsFrame = true;
    if(ev.type == gTimerEvent) {
        HandleTimerEvent(ev);
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(&ev);
    switch(ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            WindowImplSdl *w = MainWindow();
            if(w && w->onClose) w->onClose(); else gQuit = true;
            return;
        }
        default:
            ForwardToMainWindow(ev);
    }
}

static void PumpEventsOnce(bool wait) {
    SDL_Event ev;
    bool got = (wait && !gNeedsFrame) ? SDL_WaitEventTimeout(&ev, 500) : SDL_PollEvent(&ev);
    while(got) {
        HandleEvent(ev);
        got = SDL_PollEvent(&ev);
    }
}

static void DrawEditorOverlay(WindowImplSdl *w, ImVec2 origin) {
    if(!w->editorVisible) return;
    // As in guigtk.cpp start_editing(): the core passes the text baseline, so lift the entry
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
        if(w->onEditingDone) w->onEditingDone(buf);
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

static void RenderFrame() {
    if(!gSdlWindow) return;
    gNeedsFrame = false;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    WindowImplSdl *main = MainWindow();
    DrawMainMenuBar();
    DrawToolWindows();
    if(main) {
        DrawEditorOverlay(main, ImVec2(0, 0));
        DrawTooltip(main, ImVec2(0, 0));
        if(!ImGui::GetIO().WantCaptureMouse) {
            ImGui::SetMouseCursor(main->cursor == Window::Cursor::HAND ? ImGuiMouseCursor_Hand
                                                                       : ImGuiMouseCursor_Arrow);
        }
    }
    ImGui::Render();

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
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(gSdlWindow);
}

void RunGui() {
    while(!gQuit) {
        PumpEventsOnce(/*wait=*/true);
        if(gNeedsFrame) RenderFrame();
    }
}

void ExitGui() {
    gQuit = true;
}

void ClearGui() {
    if(gSdlWindow) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(gGlContext);
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = nullptr;
    }
    std::static_pointer_cast<SettingsFile>(GetSettings())->Save();
    SDL_Quit();
}

static void DrawToolWindows() {}
static void DrawMainMenuBar() {}

static bool RouteKeyDown(const SDL_Event &ev) {
    // Translate to the legacy KeyboardEvent so core accelerators work.
    WindowImplSdl *w = MainWindow();
    if(!w || !w->onKeyboardEvent) return false;
    KeyboardEvent ke = {};
    ke.type        = KeyboardEvent::Type::PRESS;
    ke.shiftDown   = (ev.key.mod & SDL_KMOD_SHIFT) != 0;
    ke.controlDown = (ev.key.mod & SDL_KMOD_CTRL) != 0;
    if(ev.key.key >= SDLK_F1 && ev.key.key <= SDLK_F12) {
        ke.key = KeyboardEvent::Key::FUNCTION;
        ke.num = 1 + (int)(ev.key.key - SDLK_F1);
    } else if(ev.key.key < 0x80) {
        ke.key = KeyboardEvent::Key::CHARACTER;
        ke.chr = (char32_t)ev.key.key;
    } else {
        return false;
    }
    return w->onKeyboardEvent(ke);
}

}
}
