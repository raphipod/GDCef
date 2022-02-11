//*************************************************************************
// Stigmee: The art to sanctuarize knowledge exchanges.
// Copyright 2021-2022 Alain Duron <duron.alain@gmail.com>
// Copyright 2021-2022 Quentin Quadrat <lecrapouille@gmail.com>
//
// This file is part of Stigmee.
//
// Stigmee is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//*************************************************************************

#include "gdcef.h"
#include "helper.hpp"
#include <wrapper/cef_helpers.h> // CEF

//------------------------------------------------------------------------------
#if defined(_WIN32)
#  define SUBPROCESS_NAME "gdcefSubProcess.exe"
#  define NEEDED_LIBRARIES "libcef.dll", "libgdcef.dll", "vulkan-1.dll", \
        "vk_swiftshader.dll", "libGLESv2.dll", "libEGL.dll"
#elif defined(__linux__)
#  define SUBPROCESS_NAME "gdcefSubProcess"
#  define NEEDED_LIBRARIES "libcef.so", "libgdcef.so", "libvulkan.so.1", \
        "libvk_swiftshader.so", "libGLESv2.so", "libEGL.so"
#elif defined(__APPLE__)
#  define SUBPROCESS_NAME "gdcefSubProcess"
#  define NEEDED_LIBRARIES "libcef.dylib", "libgdcef.dylib", "libvulkan.dylib", \
        "libvk_swiftshader.dylib", "libGLESv2.dylib", "libEGL.dylib"
#else
#  error "Undefined path for the Godot's CEF sub process for this architecture"
#endif

//------------------------------------------------------------------------------
// Init Manager static members
CefSettings GDCef::Manager::Settings;
CefMainArgs GDCef::Manager::MainArgs;
bool GDCef::Manager::CPURenderSettings;
bool GDCef::Manager::AutoPlay;

//------------------------------------------------------------------------------
void GDCef::Manager::OnBeforeCommandLineProcessing(
    const CefString& ProcessType, CefRefPtr<CefCommandLine> CommandLine)
{
    std::cout << "[GDCEF] [GDCef::Manager::OnBeforeCommandLineProcessing]" << std::endl;

    /**
     * Used to pick command line switches:
     * - If set to "true": CEF will use less CPU, but rendering performance will
     *   be lower. CSS3 and WebGL will not be usable.
     * - If set to "false": CEF will use more CPU, but rendering will be better,
     *   CSS3 and WebGL will also be usable.
     */
    GDCef::Manager::CPURenderSettings = false;
    GDCef::Manager::AutoPlay = false;

    CommandLine->AppendSwitch("off-screen-rendering-enabled");
    CommandLine->AppendSwitchWithValue("off-screen-frame-rate", "60");
    //CommandLine->AppendSwitchWithValue("force-device-scale-factor", "1");
    CommandLine->AppendSwitch("enable-font-antialiasing");
    CommandLine->AppendSwitch("enable-media-stream");

    // Should we use the render settings that use less CPU?
    if (CPURenderSettings)
    {
        CommandLine->AppendSwitch("disable-gpu");
        CommandLine->AppendSwitch("disable-gpu-compositing");
        CommandLine->AppendSwitch("enable-begin-frame-scheduling");
    }
    else
    {
        // Enables things like CSS3 and WebGL
        CommandLine->AppendSwitch("enable-gpu-rasterization");
        CommandLine->AppendSwitch("enable-webgl");
        CommandLine->AppendSwitch("disable-web-security");
    }

    CommandLine->AppendSwitchWithValue("enable-blink-features", "HTMLImports");

    if (AutoPlay)
    {
        CommandLine->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
    }

    // Append more command line options here if you want Visit Peter Beverloo's
    // site: http://peter.sh/experiments/chromium-command-line-switches/ for
    // more info on the switches
}

//------------------------------------------------------------------------------
// in a GDNative module, "_bind_methods" is replaced by the "_register_methods"
// method this is used to expose various methods of this class to Godot
void GDCef::_register_methods()
{
    std::cout << "[GDCEF] [GDCef::_register_methods]" << std::endl;

    godot::register_method("id", &GDCef::id);
    godot::register_method("is_valid", &GDCef::valid);
    godot::register_method("cef_stop", &GDCef::shutdown);
    godot::register_method("do_message_loop_work", &GDCef::doMessageLoopWork);
    godot::register_method("get_texture", &GDCef::texture);
    godot::register_method("set_zoom_level", &GDCef::setZoomLevel);
    godot::register_method("load_url", &GDCef::loadURL);
    godot::register_method("page_loaded", &GDCef::loaded);
    godot::register_method("get_url", &GDCef::getURL);
    godot::register_method("stop_loading", &GDCef::stopLoading);
    godot::register_method("can_navigate_backward", &GDCef::canNavigateBackward);
    godot::register_method("can_navigate_forward", &GDCef::canNavigateForward);
    godot::register_method("navigate_back", &GDCef::navigateBackward);
    godot::register_method("navigate_forward", &GDCef::navigateForward);
    godot::register_method("reshape", &GDCef::reshape);
    godot::register_method("on_key_pressed", &GDCef::keyPress);
    godot::register_method("on_mouse_moved", &GDCef::mouseMove);
    godot::register_method("on_mouse_left_click", &GDCef::leftClick);
    godot::register_method("on_mouse_right_click", &GDCef::rightClick);
    godot::register_method("on_mouse_middle_click", &GDCef::middleClick);
    godot::register_method("on_mouse_left_down", &GDCef::leftMouseDown);
    godot::register_method("on_mouse_left_up", &GDCef::leftMouseUp);
    godot::register_method("on_mouse_right_down", &GDCef::rightMouseDown);
    godot::register_method("on_mouse_right_up", &GDCef::rightMouseUp);
    godot::register_method("on_mouse_middle_down", &GDCef::middleMouseDown);
    godot::register_method("on_mouse_middle_up", &GDCef::middleMouseUp);
    godot::register_method("on_mouse_wheel", &GDCef::mouseWheel);
}

//------------------------------------------------------------------------------
void GDCef::_init()
{
    std::cout << "[GDCEF] [GDCef::_init] start" << std::endl;

    const std::vector<std::string> files = {
        SUBPROCESS_NAME, NEEDED_LIBRARIES,
        "icudtl.dat", "chrome_100_percent.pak", "chrome_200_percent.pak",
        "resources.pak", "v8_context_snapshot.bin"
    };

    // Get the folder path in which Stigmee and CEF assets are present
    fs::path folder;
    fs::path sub_process_path;
    fs::path sub_process_cache;

    std::cout << "[GDCEF] [GDCef::_init] Executable name: " << executable_name()  << std::endl;
    // Checking that we are not executing from the editor
    if (executable_name().find("godot") != std::string::npos)
    {
        std::cout << "[GDCEF] [GDCef::_init] launching from godot editor" << std::endl;
        folder = std::filesystem::current_path();
        std::cout << "[GDCEF] [GDCef::_init] <current_path> (where your project.godot file is located): " << folder << std::endl;
        std::cout << "[GDCEF] [GDCef::_init] All CEF libs and sub-process executables should be located in : <current_path>/build" << std::endl;
        sub_process_path = { folder / "build" / SUBPROCESS_NAME};
        sub_process_cache = { folder / "build" / "cache" };
    }
    else
    {
        std::cout << "[GDCEF] [GDCef::_init] launching from Stigmee executable" << std::endl;
        folder = real_path();
        std::cout << "[GDCEF] [GDCef::_init] <current_path> (the Stigmee executable path): " << folder << std::endl;
        std::cout << "[GDCEF] [GDCef::_init] All CEF libs and sub-process executables should be located here" << std::endl;
        sub_process_path = { folder / SUBPROCESS_NAME };
        sub_process_cache = { folder / "cache" };
        // Check if important CEF assets exist and are valid.
        if (!are_valid_files(folder, files))
        {
            std::cout << "Aborting because of missing necessary files"
                      << std::endl;
            exit(1);
        }
    }

    CefString(&GDCef::Manager::Settings.browser_subprocess_path)
            .FromString(sub_process_path.string());
    std::cout << "[GDCEF] [GDCef::_init] Looking for SubProcess at : "
              << sub_process_path.string() << std::endl;

    // Set the cache path
    CefString(&GDCef::Manager::Settings.cache_path).FromString(sub_process_cache.string());
    std::cout << "[GDCEF] [GDCef::_init] Setting cache path to : "
              << sub_process_cache.string() << std::endl;

    // Setup the default settings for GDCef::Manager
    GDCef::Manager::Settings.windowless_rendering_enabled = true;
    GDCef::Manager::Settings.no_sandbox = true;
    GDCef::Manager::Settings.remote_debugging_port = 7777;
    GDCef::Manager::Settings.uncaught_exception_stack_size = 5;

    // Make a new GDCef::Manager instance
    std::cout << "[GDCEF] [GDCef::_init] Creating the CefApp (GDCef::Manager) instance"
              << std::endl;
    CefRefPtr<GDCef::Manager> GDCefApp = new GDCef::Manager();
    CefInitialize(GDCef::Manager::MainArgs, GDCef::Manager::Settings, GDCefApp, nullptr);
    std::cout << "[GDCEF] [GDCef::_init] CefInitialize done" << std::endl;

    // Various cef settings.
    // TODO : test DPI settings
    m_window_info.SetAsWindowless(0);
    m_settings.windowless_frame_rate = 60; // 30 is default
    if (!GDCef::Manager::CPURenderSettings)
    {
        m_settings.webgl = STATE_ENABLED;
    }

    std::cout << "[GDCEF] [GDCef::_init] Creating render handler" << std::endl;
    m_render_handler = new RenderHandler(*this);
    m_client = new BrowserClient(m_render_handler);
    std::cout << "[GDCEF] [GDCef::_init] done" << std::endl;
}

//------------------------------------------------------------------------------
CefRefPtr<CefBrowser> GDCef::browser(godot::String url)
{
    if (m_browser == nullptr)
    {
        std::cout << "[GDCEF] [GDCef::browser] CreateBrowserSync" << std::endl;
        m_browser = CefBrowserHost::CreateBrowserSync(
            m_window_info, m_client.get(), url.utf8().get_data(),
            m_settings, nullptr, nullptr);
        std::cout << "[GDCEF] [GDCef::browser] CreateBrowserSync has been called !"
                  << std::endl;
        m_browser->GetHost()->WasResized();
    }
    return m_browser;
}

//------------------------------------------------------------------------------
GDCef::GDCef()
    : m_mouse_x(0), m_mouse_y(0)
{
    std::cout << "[GDCEF] [GDCef::GDCef] begin" << std::endl;
    m_image.instance();
    m_texture.instance();
}

//------------------------------------------------------------------------------
void GDCef::doMessageLoopWork()
{
    CefDoMessageLoopWork();
}

//------------------------------------------------------------------------------
GDCef::~GDCef()
{
    std::cout << "[GDCEF] [GDCef::~GDCef()]" << std::endl;
    CefDoMessageLoopWork();
    if (m_browser != nullptr)
    {
        auto host = m_browser->GetHost();
        host->CloseDevTools(); // remote_debugging_port
        host->CloseBrowser(true);
    }

    m_browser = nullptr;
    m_client = nullptr;

    // Clean Shutdown of CEF
    shutdown();
    m_render_handler = nullptr;
}

//------------------------------------------------------------------------------
GDCef::RenderHandler::RenderHandler(GDCef& owner)
    : m_owner(owner)
{}

//------------------------------------------------------------------------------
void GDCef::RenderHandler::reshape(int w, int h)
{
    std::cout << "[GDCEF] [GDCef::RenderHandler::reshape]" << std::endl;
    m_width = w;
    m_height = h;
}

//------------------------------------------------------------------------------
void GDCef::RenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
    rect = CefRect(0, 0, m_width, m_height);
}

//------------------------------------------------------------------------------
// FIXME find a less naive algorithm et utiliser dirtyRects
void GDCef::RenderHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                   const RectList& dirtyRects, const void* buffer,
                                   int width, int height)
{
    // Sanity check
    if ((width <= 0) || (height <= 0) || (buffer == nullptr))
        return ;

    // BGRA8: blue, green, red components each coded as byte
    int const COLOR_CHANELS = 4;
    int const SIZEOF_COLOR = COLOR_CHANELS * sizeof(char);
    int const TEXTURE_SIZE = SIZEOF_COLOR * width * height;

    // Copy CEF image buffer to Godot PoolVector
    m_data.resize(TEXTURE_SIZE);
    godot::PoolByteArray::Write w = m_data.write();
    memcpy(&w[0], buffer, TEXTURE_SIZE);

    // Color conversion BGRA8 -> RGBA8: swap B and R chanels
    for (int i = 0; i < TEXTURE_SIZE; i += COLOR_CHANELS)
    {
        std::swap(w[i], w[i + 2]);
    }

    // Copy Godot PoolVector to Godot texture.
    m_owner.m_image->create_from_data(width, height, false, godot::Image::FORMAT_RGBA8, m_data);
    m_owner.m_texture->create_from_image(m_owner.m_image, godot::Texture::FLAG_VIDEO_SURFACE);
}

//------------------------------------------------------------------------------
void GDCef::setZoomLevel(double delta)
{
    std::cout << "[GDCEF] [GDCef::setZoomLevel] delta:" << delta << std::endl;
    if (!m_browser)
        return;

    m_browser->GetHost()->SetZoomLevel(delta);
}

//------------------------------------------------------------------------------
void GDCef::loadURL(godot::String url)
{
    std::cout << "[GDCEF] [GDCef::loadURL]" << url.utf8().get_data() << std::endl;
    browser(url)->GetMainFrame()->LoadURL(url.utf8().get_data());
}

//------------------------------------------------------------------------------
bool GDCef::loaded()
{
    std::cout << "[GDCEF] [GDCef::loaded]" << std::endl;
    if (!m_browser)
        return false;

    return m_browser->HasDocument();
}

//------------------------------------------------------------------------------
godot::String GDCef::getURL()
{
    std::cout << "[GDCEF] [GDCef::getURL] Retrieving URL: ";
    if (m_browser && m_browser->GetMainFrame())
    {
        const char* str = m_browser->GetMainFrame()->GetURL().ToString().c_str();
        std::cout << str << std::endl;
        return str;
    }

    return {};
}

//------------------------------------------------------------------------------
void GDCef::stopLoading()
{
    std::cout << "[GDCEF] [GDCef::stopLoading]" << std::endl;
    if (!m_browser)
        return;

    m_browser->StopLoad();
}

//------------------------------------------------------------------------------
bool GDCef::canNavigateBackward()
{
    std::cerr << "[GDCEF] [GDCef::canNavigateBack]" << std::endl;
    if (!m_browser)
        return false;

    return m_browser->CanGoBack();
}

//------------------------------------------------------------------------------
void GDCef::navigateBackward()
{
    std::cout << "[GDCEF] [GDCef::navigateBack]" << std::endl;
    if ((m_browser != nullptr) && (m_browser->CanGoBack()))
    {
        m_browser->GoBack();
    }
}

//------------------------------------------------------------------------------
bool GDCef::canNavigateForward()
{
    std::cerr << "[GDCEF] [GDCef::canNavigateForward]" << std::endl;
    if (!m_browser)
        return false;

    return m_browser->CanGoForward();
}

//------------------------------------------------------------------------------
void GDCef::navigateForward()
{
    std::cout << "[GDCEF] [GDCef::navigateForward]" << std::endl;
    if ((m_browser != nullptr) && (m_browser->CanGoForward()))
    {
        m_browser->GoForward();
    }
}

//------------------------------------------------------------------------------
void GDCef::reshape(int w, int h)
{
    std::cout << "[GDCEF] [GDCef::reshape]" << std::endl;
    if (!m_browser || !m_browser->GetHost())
        return;

    std::cout << "[GDCEF] [GDCef::reshape] m_render_handler->reshape" << std::endl;

    m_render_handler->reshape(w, h);
    std::cout << "[GDCEF] [GDCef::reshape] m_browser->GetHost()->WasResized" << std::endl;
    m_browser->GetHost()->WasResized();
}

//------------------------------------------------------------------------------
int GDCef::id()
{
    std::cerr << "[GDCEF][GDCef::id]" << std::endl;
    if (!m_browser)
        return -1;

    return m_browser->GetIdentifier();
}

//------------------------------------------------------------------------------
bool GDCef::valid()
{
    std::cerr << "[GDCEF][GDCef::valid]" << std::endl;
    if (!m_browser)
        return false;

    return m_browser->IsValid();
}

//------------------------------------------------------------------------------
void GDCef::shutdown()
{
    std::cerr << "[GDCEF][GDCef::cefStop]" << std::endl;
    CefShutdown();
}
