#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#if __has_include("ImageItem.g.cpp")
#include "ImageItem.g.cpp"
#endif
#if __has_include("HistoryItem.g.cpp")
#include "HistoryItem.g.cpp"
#endif

#include "microsoft.ui.xaml.window.h"
#include "winrt/Microsoft.UI.Interop.h"
#include "winrt/Microsoft.UI.Windowing.h"
#include "winrt/Microsoft.Windows.AppLifecycle.h"

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Windowing;

namespace muxc = Microsoft::UI::Xaml::Controls;
namespace utils = Xellanix::Utilities;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::ImageMerger::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        m_imageItems = single_threaded_observable_vector<Windows::Foundation::IInspectable>();
        m_historyItems = single_threaded_observable_vector<Windows::Foundation::IInspectable>();

        SetModernAppTitleBar();

        SetBackground();

        m_closedRevoker = this->Closed(winrt::auto_revoke, [&](auto&&, auto&&)
        {
            if (nullptr != m_backdropController)
            {
                m_backdropController.Close();
                m_backdropController = nullptr;
            }

            if (nullptr != m_dispatcherQueueController)
            {
                m_dispatcherQueueController.ShutdownQueueAsync();
                m_dispatcherQueueController = nullptr;
            }
        });
    }

    #pragma region Modern Title Section
    void MainWindow::SetModernAppTitleBar()
    {
        auto windowNative{ this->try_as<::IWindowNative>() };
        winrt::check_bool(windowNative);
        windowNative->get_WindowHandle(&Xellanix::Desktop::WindowHandle);

        Microsoft::UI::WindowId windowId =
            Microsoft::UI::GetWindowIdFromWindow(Xellanix::Desktop::WindowHandle);

        Microsoft::UI::Windowing::AppWindow appWindow =
            Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);

        if (AppWindowTitleBar::IsCustomizationSupported())
        {
            auto titleBar{ appWindow.TitleBar() };
            titleBar.ExtendsContentIntoTitleBar(true);

            RightPaddingColumn().Width(GridLengthHelper::FromPixels(titleBar.RightInset()));
            LeftPaddingColumn().Width(GridLengthHelper::FromPixels(titleBar.LeftInset()));

            titleBar.ButtonBackgroundColor(Microsoft::UI::Colors::Transparent());
            titleBar.ButtonInactiveBackgroundColor(Microsoft::UI::Colors::Transparent());
        }
        else
        {
            // Title bar customization using these APIs is currently
            // supported only on Windows 11. In other cases, hide
            // the custom title bar element.
            AppTitleBar().Visibility(Visibility::Collapsed);

            // Show alternative UI for any functionality in
            // the title bar, such as search.
        }

        if (appWindow)
        {
            // You now have an AppWindow object, and you can call its methods to manipulate the window.
            // As an example, let's change the title text of the window.
            appWindow.Title(L"Xellanix ImageMerger");
        }
    }
    #pragma endregion

    #pragma region Mica Material Background
    void MainWindow::SetBackground()
    {
        if (winrt::MUCSB::MicaController::IsSupported())
        {
            // We ensure that there is a Windows.System.DispatcherQueue on the current thread.
            // Always check if one already exists before attempting to create a new one.
            if (nullptr == winrt::WS::DispatcherQueue::GetForCurrentThread() &&
                nullptr == m_dispatcherQueueController)
            {
                m_dispatcherQueueController = CreateSystemDispatcherQueueController();
            }

            // Setup the SystemBackdropConfiguration object.
            SetupSystemBackdropConfiguration();

            // Setup Mica on the current Window.
            m_backdropController = winrt::MUCSB::MicaController();
            m_backdropController.SetSystemBackdropConfiguration(m_configuration);
            m_backdropController.AddSystemBackdropTarget(
                this->try_as<winrt::MUC::ICompositionSupportsSystemBackdrop>());
        }
        else
        {
            // The backdrop material is not supported.
        }
    }

    winrt::WS::DispatcherQueueController MainWindow::CreateSystemDispatcherQueueController()
    {
        DispatcherQueueOptions options
        {
            sizeof(DispatcherQueueOptions),
            DQTYPE_THREAD_CURRENT,
            DQTAT_COM_NONE
        };

        ::ABI::Windows::System::IDispatcherQueueController* ptr{ nullptr };
        winrt::check_hresult(CreateDispatcherQueueController(options, &ptr));
        return { ptr, take_ownership_from_abi };
    }

    void MainWindow::SetupSystemBackdropConfiguration()
    {
        m_configuration = winrt::MUCSB::SystemBackdropConfiguration();

        // Activation state.
        m_activatedRevoker = this->Activated(winrt::auto_revoke,
                                             [&](auto&&, MUX::WindowActivatedEventArgs const& args)
        {
            m_configuration.IsInputActive(
                winrt::MUX::WindowActivationState::Deactivated != args.WindowActivationState());
        });

        // Initial state.
        m_configuration.IsInputActive(true);

        // Application theme.
        m_rootElement = this->Content().try_as<winrt::MUX::FrameworkElement>();
        if (nullptr != m_rootElement)
        {
            m_themeChangedRevoker = m_rootElement.ActualThemeChanged(winrt::auto_revoke,
                                                                     [&](auto&&, auto&&)
            {
                m_configuration.Theme(
                    ConvertToSystemBackdropTheme(m_rootElement.ActualTheme()));

                if (Application::Current().RequestedTheme() == ApplicationTheme::Light)
                {
                    SetSystemButtonTheme(false);
                }
                else
                {
                    SetSystemButtonTheme(true);
                }
            });

            // Initial state.
            m_configuration.Theme(
                ConvertToSystemBackdropTheme(m_rootElement.ActualTheme()));
        }
    }

    winrt::MUCSB::SystemBackdropTheme MainWindow::ConvertToSystemBackdropTheme(winrt::MUX::ElementTheme const& theme)
    {
        switch (theme)
        {
            case winrt::MUX::ElementTheme::Dark:
                return winrt::MUCSB::SystemBackdropTheme::Dark;
            case winrt::MUX::ElementTheme::Light:
                return winrt::MUCSB::SystemBackdropTheme::Light;
            default:
                return winrt::MUCSB::SystemBackdropTheme::Default;
        }
    }
    #pragma endregion

    #pragma region Window Theme
    void MainWindow::SetSystemButtonTheme(bool isDarkMode)
    {
        BOOL value = isDarkMode;
        auto windowNative{ this->try_as<::IWindowNative>() };
        winrt::check_bool(windowNative);
        windowNative->get_WindowHandle(&Xellanix::Desktop::WindowHandle);

        ::DwmSetWindowAttribute(Xellanix::Desktop::WindowHandle, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

        Microsoft::UI::WindowId windowId =
            Microsoft::UI::GetWindowIdFromWindow(Xellanix::Desktop::WindowHandle);

        Microsoft::UI::Windowing::AppWindow appWindow =
            Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);

        if (AppWindowTitleBar::IsCustomizationSupported())
        {
            auto titleBar{ appWindow.TitleBar() };

            auto foregroundColor{ Microsoft::UI::ColorHelper::FromArgb(255, 48, 48, 48) };
            auto hoverBackground{ Microsoft::UI::ColorHelper::FromArgb(255, 230, 230, 230) };
            auto preseedBackground{ Microsoft::UI::ColorHelper::FromArgb(255, 242, 242, 242) };
            if (isDarkMode)
            {
                foregroundColor = Microsoft::UI::ColorHelper::FromArgb(255, 245, 245, 247);
                hoverBackground = Microsoft::UI::ColorHelper::FromArgb(255, 51, 51, 51);
                preseedBackground = Microsoft::UI::ColorHelper::FromArgb(255, 38, 38, 38);
            }

            titleBar.ButtonForegroundColor(foregroundColor);
            titleBar.ButtonHoverForegroundColor(foregroundColor);
            titleBar.ButtonHoverBackgroundColor(hoverBackground);
            titleBar.ButtonPressedForegroundColor(foregroundColor);
            titleBar.ButtonPressedBackgroundColor(preseedBackground);
            titleBar.ButtonInactiveForegroundColor(foregroundColor);
        }
    }
    #pragma endregion

    Windows::Foundation::IAsyncAction MainWindow::ProcessActivationInfo()
    {
        using namespace Microsoft::Windows::AppLifecycle;

        apartment_context ui_thread;
        auto weak{ get_weak() };
        co_await resume_background();

        AppActivationArguments args = AppInstance::GetCurrent().GetActivatedEventArgs();
        if (args.Kind() == ExtendedActivationKind::Launch)
        {
            if (auto launchArgs{ args.Data().try_as<Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs>() })
            {
                std::wstring argString{ launchArgs.Arguments() };
                
                std::wstring verb;
                auto pos = argString.find(L"--");
                if (pos == std::wstring::npos) co_return;
                {
                    auto space = argString.find(L" ", pos + 2);
                    verb = argString.substr(pos, space);

                    argString = argString.substr(space + 1);
                }

                std::vector<std::wstring> files;
                files.reserve(1'000'000);

                std::wistringstream iss(argString);
                for (std::wstring s; iss >> std::quoted(s,L'"', L'\000'); )
                {
                    files.emplace_back(s);
                }
                files.shrink_to_fit();

                natural_sort(files);
                if (auto strong{ weak.get() })
                {
                    co_await ui_thread;
                    for (auto const& s : files)
                    {
                        strong->FromFile(s);
                    }
                }
            }
        }
    }
    void MainWindow::FromFile(param::hstring const& filepath)
    {
        std::wstring fpath{ hstring{ filepath } };

        if (utils::CheckExist(fpath))
        {
            auto fname = get_filename(fpath);

            auto newItem = make<ImageItem>(hstring(fname), hstring(fpath));
            m_imageItems.Append(newItem);

            if (!_autoSelect) return;
            if (auto list{ ImageList() })
            {
                list.SelectRange(Data::ItemIndexRange(m_imageItems.Size() - 1, 1));
            }
        }
    }
    Windows::Foundation::IAsyncAction MainWindow::ReloadHistory(std::wstring const& data)
    {
        auto strong{ get_strong() };

        co_await resume_background();

        auto cancellation = co_await get_cancellation_token();
        cancellation.enable_propagation();

        size_t totalLines = std::count(data.begin(), data.end(), L'\n');
        if (data.back() != L'\n' && data.back() != L'\0' && data.size() > 0) ++totalLines;

        if (cancellation()) co_return;

        auto itemToShow = (std::min)((size_t)strong->_maxHistory, totalLines);

        strong->m_historyItems.Clear();

        for (size_t i = 0; i < itemToShow; i++)
        {
            if (cancellation()) co_return;

            auto h_path = GetStringByLine(data, totalLines - 1 - i);
            time_t h_mtime = 0;
            long long h_size = 0;

            get_fattr(h_path, h_mtime, h_size);

            auto [s_num, s_scale] = utils::NormalizeBytes((double)h_size);

            strong->m_historyItems.Append(make<HistoryItem>(hstring(get_filename(h_path)),
                                          hstring(h_path),
                                          hstring(utils::NumFormatter(s_num) + s_scale),
                                          hstring(utils::TimeString(utils::localtime_x(h_mtime), L"%a, %B %d, %Y %I:%M %p"))));
        }
    }

    void MainWindow::Grid_Loaded(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        using namespace std::chrono_literals;
        using namespace utils;

        fileChecker.Interval(500ms);
        if (!fileChecker_revoker)
        {
            fileChecker_revoker = fileChecker.Tick(auto_revoke, [&](auto&&, auto&&)
            {
                struct _stati64 current_buf;
            auto res = _wstat64((utils::LocalAppData / L"Settings\\ImageMerger.xsmf").wstring().c_str(), &current_buf);

            if (res == 0)
            {
                if (current_buf.st_ctime != last_ctime1 ||
                    current_buf.st_mtime != last_mtime1 ||
                    current_buf.st_size != last_size1)
                {
                    LoadSettings();

                    last_ctime1 = current_buf.st_ctime;
                    last_mtime1 = current_buf.st_mtime;
                    last_size1 = current_buf.st_size;
                }
            }
            });
        }
        fileChecker.Start();

        constexpr bool morethan0 = false;
        DeleteImageBtn().IsEnabled(morethan0);
        MergeImageBtn().IsEnabled(morethan0);
        ClearImageBtn().IsEnabled(morethan0);
        SelectAllBtn().IsEnabled(morethan0);
        NoFilePanel().Visibility(Visibility::Visible);

        if (!vectorToken)
        {
            vectorToken = m_imageItems.VectorChanged(auto_revoke, [&](auto&&, auto&&)
            {
                bool _morethan0 = m_imageItems.Size() > 0 ? true : false;
                ClearImageBtn().IsEnabled(_morethan0);
                SelectAllBtn().IsEnabled(_morethan0);

                if (_morethan0)
                {
                    NoFilePanel().Visibility(Visibility::Collapsed);
                }
                else
                {
                    NoFilePanel().Visibility(Visibility::Visible);
                }
            });
        }

        checkProcess.Interval(1s);
        if (!tick_revoker)
        {
            tick_revoker = checkProcess.Tick(auto_revoke, [&](auto&&, auto&&)
            {
                bool isReqToClose = false;
                if (!IsProcessRunning(pi))
                {
                    if (auto info{ MergingInfo() })
                    {
                        if (CheckExist(last_file_path))
                        {
                            // Check date created or modif was after last operation
                            // if not it was failed!

                            time_t dc = 0, dm = 0;
                            long long ds = 0;
                            {
                                struct _stati64 buf;
                                if (_wstat64(last_file_path.c_str(), &buf) == 0)
                                {
                                    dc = buf.st_ctime;
                                    dm = buf.st_mtime;
                                    ds = buf.st_size;
                                }
                            }

                            if ((dc >= last_op_t || dm >= last_op_t) && ds > 0)
                            {
                                // Success!
                                info.IsOpen(false);

                                info.Severity(muxc::InfoBarSeverity::Success);
                                info.Title(L"Merging Success!");
                                info.Message(L"last operation time: " + last_op);
                                info.ActionButton().Visibility(Visibility::Visible);
                                info.ActionButton().IsEnabled(true);
                                info.IsOpen(true);

                                if (_autoOpen)
                                {
                                    ShellExecute(NULL, NULL, last_file_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
                                }
                                if (_autoReveal)
                                {
                                    ShellExecute(NULL, NULL, L"explorer.exe", (L"/select,\"" + last_file_path + L"\"").c_str(), NULL, SW_SHOWNORMAL);
                                }
                                if (_autoClose)
                                {
                                    isReqToClose = true;
                                }

                                CheckAndAppend(last_file_path.c_str());
                            }
                            else
                            {
                                // Failed!
                                info.IsOpen(false);

                                info.Severity(muxc::InfoBarSeverity::Error);
                                info.Title(L"Merging Failed!");
                                info.Message(L"last operation time: " + last_op);
                                info.ActionButton().Visibility(Visibility::Collapsed);
                                info.ActionButton().IsEnabled(false);
                                info.IsOpen(true);
                            }
                        }
                        else
                        {
                            // Failed to create file
                            info.IsOpen(false);

                            info.Severity(muxc::InfoBarSeverity::Error);
                            info.Title(L"Merging Failed!");
                            info.Message(L"last operation time: " + last_op);
                            info.ActionButton().Visibility(Visibility::Collapsed);
                            info.ActionButton().IsEnabled(false);
                            info.IsOpen(true);
                        }
                    }

                    if (auto content{ WindowContent() })
                    {
                        content.IsEnabled(true);
                    }
                    MergingBar().IsIndeterminate(false);

                    checkProcess.Stop();

                    if (pi.hProcess != NULL && CloseHandle(pi.hProcess))
                    {
                        pi.hProcess = NULL;
                    }
                    if (pi.hThread != NULL && CloseHandle(pi.hThread))
                    {
                        pi.hThread = NULL;
                    }
                }

                if (isReqToClose)
                {
                    [&]() -> fire_and_forget
                    {
                        co_await[]() -> Windows::Foundation::IAsyncAction
                        {
                            co_await 3s;
                        }();
                        Application::Current().Exit();
                    }();
                }
            });
        }

        m_imageItems.Clear();
        no_await(ProcessActivationInfo());
    }
    void MainWindow::Window_Closed(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::WindowEventArgs const& /*args*/)
    {
        checkProcess.Stop();
        tick_revoker.~event_revoker();
        checkProcess = nullptr;

        if (pi.hProcess != NULL || pi.hThread != NULL)
        {
            if (CloseHandle(pi.hProcess))
            {
                pi.hProcess = NULL;
            }
            if (CloseHandle(pi.hThread))
            {
                pi.hThread = NULL;
            }
        }
        pi = PROCESS_INFORMATION{};

        fileChecker.Stop();
        fileChecker_revoker.~event_revoker();
        fileChecker = nullptr;
    }
    Windows::Foundation::IAsyncAction MainWindow::PivotSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        if (auto pivot{ sender.try_as<Controls::Pivot>() })
        {
            if (pivot.SelectedIndex() == 1)
            {
                // Activate history
                if (std::wstring data; GetHistoryData(data))
                {
                    if (!data.empty())
                    {
                        try_cancel(reloadHistoryOp);
                        reloadHistoryOp = ReloadHistory(data);
                        co_await reloadHistoryOp;

                        co_return;
                    }
                }
            }
        }

        try_cancel(reloadHistoryOp);
        m_historyItems.Clear();
    }

    #pragma region InfoBar Events
    void MainWindow::OpenResultClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        ShellExecute(NULL, NULL, last_file_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    #pragma endregion

    #pragma region ImageList Events
    void MainWindow::ImageSelectionChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        auto items{ ImageList().SelectedRanges() };

        // Enable or Disable Delete and Merge Button basedon 'items' size
        bool morethan0 = items.Size() > 0 ? true : false;
        if (auto btn{ MergeImageBtn() })
        {
            if (btn.IsEnabled() == morethan0) return;

            DeleteImageBtn().IsEnabled(morethan0);
            btn.IsEnabled(morethan0);
        }
    }

    void MainWindow::FilesDragOver(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::DragEventArgs const& e)
    {
        e.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
    }

    Windows::Foundation::IAsyncAction MainWindow::FilesDrop(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::DragEventArgs const& e)
    {
        auto weak{ get_weak() };

        if (e.DataView().Contains(Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
        {
            auto&& items{ co_await e.DataView().GetStorageItemsAsync() };
            if (items.Size() == 0) co_return;

            for (auto&& item : items)
            {
                if (!item.IsOfType(Windows::Storage::StorageItemTypes::File)) continue;

                if (auto file{ item.try_as<Windows::Storage::StorageFile>() })
                {
                    auto fileExt{ file.FileType() };
                    if (fileExt == L".jpg" ||
                        fileExt == L".jpeg" ||
                        fileExt == L".png" ||
                        fileExt == L".bmp" ||
                        fileExt == L".tif" ||
                        fileExt == L".tiff" ||
                        fileExt == L".pdf")
                    {
                        if (auto strong{ weak.get() }) strong->FromFile(file.Path());
                    }
                }
            }
        }
    }
    #pragma endregion

    #pragma region ControlBar Events
    Windows::Foundation::IAsyncAction MainWindow::AddClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto weak{ get_weak() };

        Windows::Storage::Pickers::FileOpenPicker picker{};
        picker.as<Xellanix::Desktop::IInitializeWithWindow>()->Initialize(Xellanix::Desktop::WindowHandle);
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        picker.FileTypeFilter().Append(L".png");
        picker.FileTypeFilter().Append(L".jpg");
        picker.FileTypeFilter().Append(L".jpeg");
        picker.FileTypeFilter().Append(L".bmp");
        picker.FileTypeFilter().Append(L".tif");
        picker.FileTypeFilter().Append(L".tiff");
        picker.FileTypeFilter().Append(L".pdf");
        auto&& files{ co_await picker.PickMultipleFilesAsync() };

        if (!files) co_return;

        if (auto strong{ weak.get() })
        {
            for (auto&& file : files)
            {
                strong->FromFile(file.Path());
            }
        }
    }

    void MainWindow::DeleteItemClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto list{ ImageList() };
        if (!list) return;

        std::vector<uint32_t> removed;
        removed.reserve(list.Items().Size());

        for (auto&& i : list.SelectedRanges())
        {
            uint32_t start{ static_cast<uint32_t>(i.FirstIndex()) };
            for (uint32_t index = start; index < start + i.Length(); index++)
            {
                removed.emplace_back(index);
            }
        }

        removed.shrink_to_fit();
        std::reverse(removed.begin(), removed.end());

        for (auto& i : removed)
        {
            m_imageItems.RemoveAt(i);
        }
    }

    void MainWindow::ClearItemClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        m_imageItems.Clear();
    }

    void MainWindow::SelectAllClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto list{ ImageList() };
        if (!list) return;

        list.SelectAll();
    }

    void MainWindow::DeselectClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto list{ ImageList() };
        if (!list) return;

        std::vector<Microsoft::UI::Xaml::Data::ItemIndexRange> ranges;
        for (auto&& range : list.SelectedRanges())
        {
            ranges.emplace_back(range);
        }
        for (size_t i = 0; i < ranges.size(); i++)
        {
            list.DeselectRange(ranges[i]);
        }
    }

    Windows::Foundation::IAsyncAction MainWindow::MergeItemClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto strong_this{ get_strong() };
        strong_this->last_op_t = time(0);
        strong_this->last_op = utils::TimeString(utils::localtime_x(strong_this->last_op_t), L"%a, %B %d, %Y, %H:%M:%S");

        Windows::Storage::Pickers::FileSavePicker picker{};
        picker.as<Xellanix::Desktop::IInitializeWithWindow>()->Initialize(Xellanix::Desktop::WindowHandle);
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        picker.SuggestedFileName(L"Merge Result");
        if (strong_this->defaultType == L".png")
        {
            picker.FileTypeChoices().Insert(L"PNG File", single_threaded_vector<hstring>({ L".png" }));
            picker.FileTypeChoices().Insert(L"JPEG File", single_threaded_vector<hstring>({ L".jpeg", L".jpg" }));
            picker.FileTypeChoices().Insert(L"PDF File", single_threaded_vector<hstring>({ L".pdf" }));
        }
        else if (strong_this->defaultType == L".jpeg")
        {
            picker.FileTypeChoices().Insert(L"JPEG File", single_threaded_vector<hstring>({ L".jpeg", L".jpg" }));
            picker.FileTypeChoices().Insert(L"PNG File", single_threaded_vector<hstring>({ L".png" }));
            picker.FileTypeChoices().Insert(L"PDF File", single_threaded_vector<hstring>({ L".pdf" }));
        }
        else if (strong_this->defaultType == L".pdf")
        {
            picker.FileTypeChoices().Insert(L"PDF File", single_threaded_vector<hstring>({ L".pdf" }));
            picker.FileTypeChoices().Insert(L"PNG File", single_threaded_vector<hstring>({ L".png" }));
            picker.FileTypeChoices().Insert(L"JPEG File", single_threaded_vector<hstring>({ L".jpeg", L".jpg" }));
        }

        auto&& file{ co_await picker.PickSaveFileAsync() };
        if (file)
        {
            strong_this->last_file_path = file.Path().c_str();

            strong_this->MergingBar().IsIndeterminate(true);

            auto list{ strong_this->ImageList() };
            if (!list) co_return;

            if (auto content{ WindowContent() })
            {
                content.IsEnabled(false);
            }

            if (auto info{ strong_this->MergingInfo() })
            {
                info.IsOpen(false);

                info.Severity(muxc::InfoBarSeverity::Warning);
                info.Title(L"Warning: ");
                info.Message(L"Don't move to another page during the merging process! Moving to another page may cause the merge process to be aborted.");
                info.ActionButton().Visibility(Visibility::Collapsed);
                info.ActionButton().IsEnabled(false);

                info.IsOpen(true);
            }

            // Contruct images path list and config file
            {
                std::locale::global(std::locale(std::locale("C"), new cvt::utf8cvt<wchar_t>()));

                const fs::path appft = utils::LocalAppData / L"pdftools";
                if (!fs::exists(appft))
                    fs::create_directories(appft);
                const std::wstring ip = (appft / L"images.txt").wstring();
                const std::wstring cp = (appft / L"config.txt").wstring();
                std::wofstream iwof(ip), cwof(cp);

                cwof << LR"(d6bb8c8c-d255-46af-9d2d-11ee63f8d85a : )" << std::wstring(file.FileType().c_str()).substr(1) << L"\n";
                cwof << LR"(b4897706-b448-4ca2-9322-4d8cdc474ef5 : )" << strong_this->last_file_path << L"\n";
                cwof << LR"(6814ba7a-45f3-47df-907e-bbcfda04e924 : )" << strong_this->_expansionMode << L"\n";
                cwof << LR"(f7d64306-d529-4972-aaa1-75d622626acc : )" << (strong_this->_useCompression ? L"true" : L"false") << L"\n";
                cwof << LR"(0019f40c-feba-42de-875a-10d7a8626e2e : )" << std::to_wstring(strong_this->_compressionIntensity);
                {
                    std::vector<std::wstring> simages;
                    simages.reserve(list.Items().Size());
                    for (auto&& range : list.SelectedRanges())
                    {
                        uint32_t start{ static_cast<uint32_t>(range.FirstIndex()) };
                        for (uint32_t index = start; index < start + range.Length(); index++)
                        {
                            if (auto imgitem{ list.Items().GetAt(index).try_as<ImageMerger::ImageItem>() })
                            {
                                simages.emplace_back(imgitem.ItemPath().c_str());
                            }
                        }
                    }

                    simages.shrink_to_fit();

                    for (size_t i = 0; i < simages.size(); i++)
                    {
                        iwof << simages[i];
                        if (i + 1 < simages.size()) iwof << L"\n";
                    }
                }

                iwof.close();
                cwof.close();
            }

            STARTUPINFO si;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&strong_this->pi, sizeof(strong_this->pi));

            //Prepare CreateProcess args
            const std::wstring app_w = (fs::path(utils::AppDir) / L"pdftools\\pdftools.exe").wstring();
            const std::wstring arg_w = (L"\"" + app_w +
                                        L"\" \"" +
                                        (utils::LocalAppData / L"pdftools").wstring() +
                                        L"\" \"" +
                                        (utils::LocalTemp / L"pdftools").wstring() +
                                        L"\"");

            if (!CreateProcess(app_w.c_str(), const_cast<wchar_t*>(arg_w.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &strong_this->pi))
            {
                if (auto content{ WindowContent() })
                {
                    content.IsEnabled(true);
                }
                strong_this->MergingBar().IsIndeterminate(false);

                strong_this->checkProcess.Stop();

                if (auto info{ strong_this->MergingInfo() })
                {
                    info.IsOpen(false);

                    info.Severity(muxc::InfoBarSeverity::Error);
                    info.Title(L"Merging Failed!");
                    info.Message(L"last operation time: " + strong_this->last_op);
                    info.ActionButton().Visibility(Visibility::Collapsed);
                    info.ActionButton().IsEnabled(false);
                    info.IsOpen(true);
                }
            }
            else
            {
                strong_this->checkProcess.Start();
            }
        }
    }
    #pragma endregion

    #pragma region Setting Manager
    void MainWindow::LoadSettings()
    {
        isloaded = false;

        if (settings.Read(utils::LocalAppData / L"Settings\\ImageMerger.xsmf"))
        {
            try
            {
                defaultType = (settings >> L"0b96051e-d349-5ed2-b747-55e4dff705b0").try_as<std::wstring>(L".pdf");
                _useCompression = (settings >> L"32a7dff8-7bc2-51af-aec0-74b5bb63f4c3").try_as<bool>(false);
                _compressionIntensity = (std::min)((settings >> L"f7ecd462-6ab5-5904-b95f-1a2c58f5519c").try_as<unsigned short>(25ui16), 100ui16);
                _expansionMode = (settings >> L"12b83ac9-aa3e-5ce4-8eee-fc9bbc80edad").try_as<std::wstring>(L"vertical");
                _autoSelect = (settings >> L"e1e69d9b-0be1-5f94-8745-b0ebc4b30da1").try_as<bool>(true);
                _autoOpen = (settings >> L"b1a682da-bb85-34ee-9500-6c8cb805ae8a").try_as<bool>(true);
                _autoReveal = (settings >> L"fc605f4d-435c-582a-a9a3-ceb16a1f037e").try_as<bool>(false);
                _autoClose = (settings >> L"b68c9c06-090a-54ec-99e9-5cf73ee29fc4").try_as<bool>(false);
                _maxHistory = (settings >> L"dfef3e33-ff80-50fd-b9a9-895f57139646").try_as<double>(10.0);
            }
            catch (...)
            {}

            if (defaultType == L".pdf")
            {
                DefaultExtension().SelectedIndex(0);
            }
            else if (defaultType == L".png")
            {
                DefaultExtension().SelectedIndex(1);
            }
            else if (defaultType == L".jpeg")
            {
                DefaultExtension().SelectedIndex(2);
            }
            else
            {
                DefaultExtension().SelectedIndex(0);
            }
            UseCompression().IsOn(_useCompression);
            CompressionIntensity().Value(_compressionIntensity);
            ExpansionMode().SelectedIndex(_expansionMode == L"horizontal" ? 0 : 1);
            AutoSelect().IsOn(_autoSelect);
            AutoOpen().IsOn(_autoOpen);
            AutoReveal().IsOn(_autoReveal);
            AutoClose().IsOn(_autoClose);
            MaxHistory().Value(_maxHistory);
        }
        else
        {
            settings[L"0b96051e-d349-5ed2-b747-55e4dff705b0"] = defaultType;
            settings[L"32a7dff8-7bc2-51af-aec0-74b5bb63f4c3"] = _useCompression;
            settings[L"f7ecd462-6ab5-5904-b95f-1a2c58f5519c"] = _compressionIntensity;
            settings[L"12b83ac9-aa3e-5ce4-8eee-fc9bbc80edad"] = _expansionMode;
            settings[L"e1e69d9b-0be1-5f94-8745-b0ebc4b30da1"] = _autoSelect;
            settings[L"b1a682da-bb85-34ee-9500-6c8cb805ae8a"] = _autoOpen;
            settings[L"fc605f4d-435c-582a-a9a3-ceb16a1f037e"] = _autoReveal;
            settings[L"b68c9c06-090a-54ec-99e9-5cf73ee29fc4"] = _autoClose;
            settings[L"dfef3e33-ff80-50fd-b9a9-895f57139646"] = _maxHistory;
        }

        isloaded = true;
    }

    template<typename T>
    typename std::enable_if<std::bool_constant<std::_Is_any_of_v<std::remove_cv_t<T>, bool, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, std::wstring>>::value>::type
        MainWindow::SaveSettings(std::wstring const& name, T const& value)
    {
        settings[name] = value;

        if (m_source == 1) return;

        settings.Write(utils::LocalAppData / L"Settings\\ImageMerger.xsmf");
        get_ftime(L"Settings\\ImageMerger.xsmf", last_ctime1, last_mtime1, last_size1);
    }

    #pragma endregion

    #pragma region Merging Option Events
    void MainWindow::SettingsSourceChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        m_source = SettingsSource().SelectedIndex();
        if (m_source == 1)
        {
            // Local Window Settings
            fileChecker.Stop();
            last_ctime1 = last_mtime1 = 0;
            last_size1 = 0;
        }
        else
        {
            // Global Settings (m_source == 0 (or anything not 1))
            fileChecker.Start();
        }
    }

    void MainWindow::DefaultExtensionChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        auto index = DefaultExtension().SelectedIndex();
        switch (index)
        {
            case 0: // PDF
                defaultType = L".pdf";
                break;
            case 1: // PNG
                defaultType = L".png";
                break;
            case 2: // JPEG
                defaultType = L".jpeg";
                break;
            default: // PDF
                defaultType = L".pdf";
                break;
        }

        SaveSettings(L"0b96051e-d349-5ed2-b747-55e4dff705b0", defaultType);
    }

    void MainWindow::UseCompressionChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        _useCompression = UseCompression().IsOn();

        SaveSettings(L"32a7dff8-7bc2-51af-aec0-74b5bb63f4c3", _useCompression);
    }

    void MainWindow::IntensityChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args)
    {
        if (!isloaded) return;

        auto val = (std::min)(static_cast<unsigned short>(sender.Text() == L"" ? args.OldValue() : sender.Value()), 100ui16);
        sender.Value(val);

        _compressionIntensity = val;

        SaveSettings(L"f7ecd462-6ab5-5904-b95f-1a2c58f5519c", _compressionIntensity);
    }

    void MainWindow::ExpansionModeChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        _expansionMode = ExpansionMode().SelectedIndex() == 0 ? L"horizontal" : L"vertical";

        SaveSettings(L"12b83ac9-aa3e-5ce4-8eee-fc9bbc80edad", _expansionMode);
    }

    void MainWindow::AutoSelectChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        _autoSelect = AutoSelect().IsOn();

        SaveSettings(L"e1e69d9b-0be1-5f94-8745-b0ebc4b30da1", _autoSelect);
    }

    void MainWindow::AutoOpenChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        _autoOpen = AutoOpen().IsOn();

        SaveSettings(L"b1a682da-bb85-34ee-9500-6c8cb805ae8a", _autoOpen);
    }

    void MainWindow::AutoRevealChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        _autoReveal = AutoReveal().IsOn();

        SaveSettings(L"fc605f4d-435c-582a-a9a3-ceb16a1f037e", _autoReveal);
    }

    void MainWindow::AutoCloseChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!isloaded) return;

        _autoClose = AutoClose().IsOn();

        SaveSettings(L"b68c9c06-090a-54ec-99e9-5cf73ee29fc4", _autoClose);
    }
    #pragma endregion

    #pragma region History Option Events
    void MainWindow::MaxHistoryChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args)
    {
        if (!isloaded) return;

        auto val = (std::max)(utils::round_up(sender.Text() == L"" ? args.OldValue() : sender.Value(), 0ui16), 5.0);
        sender.Value(val);

        _maxHistory = val;

        SaveSettings(L"dfef3e33-ff80-50fd-b9a9-895f57139646", _maxHistory);
    }
    #pragma endregion

    #pragma region History Items Events
    Windows::Foundation::IAsyncAction MainWindow::RemoveHistoryData(std::wstring const& path)
    {
        if (std::wstring data; GetHistoryData(data))
        {
            if (data.empty()) co_return;

            auto toRemove{ path };
            if (auto pos = findCaseInsensitive(data, toRemove); pos != std::wstring::npos)
            {
                removeline(data, toRemove, pos);
                MakeHistoryData(data);

                try_cancel(reloadHistoryOp);
                reloadHistoryOp = ReloadHistory(data);
                co_await reloadHistoryOp;
            }
        }
        else
        {
            m_historyItems.Clear();
        }
    }

    Windows::Foundation::IAsyncAction MainWindow::RemoveHistory(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        std::wstring path{ unbox_value_or<hstring>(sender.try_as<Controls::Button>().Tag(), L"") };
        if (path == L"") co_return;

        co_await RemoveHistoryData(path);
    }

    void MainWindow::ShowContextMenu(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (auto obj{ sender.try_as<DependencyObject>() })
        {
            if (auto item{ find_ancestor<Controls::ListViewItem>(obj) })
            {
                item.ContextFlyout().ShowAt(item);
            }
        }
    }

    Windows::Foundation::IAsyncAction MainWindow::DeleteByClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        hstring path = unbox_value_or<hstring>(sender.try_as<Controls::MenuFlyoutItem>().Tag(), L"");
        if (path == L"") co_return;

        Controls::ContentDialog dialog{};

        dialog.XamlRoot(m_rootElement.XamlRoot());
        dialog.Style(unbox_value<Style>(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle"))));
        dialog.Title(box_value(L"Delete this file?"));
        dialog.PrimaryButtonText(L"Delete");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(Controls::ContentDialogButton::Primary);
        dialog.Content(box_value(path));

        auto&& result = co_await dialog.ShowAsync();
        if (result == Controls::ContentDialogResult::Primary)
        {
            std::wstring wspath{ path };
            if (fs::remove(fs::path(wspath)))
            {
                // Deleted
                co_await RemoveHistoryData(wspath);
            }
        }
    }

    void MainWindow::RevealMergedFile(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        hstring path = unbox_value_or<hstring>(sender.try_as<Controls::MenuFlyoutItem>().Tag(), L"");
        if (path == L"") return;

        ShellExecute(NULL, NULL, L"explorer.exe", (L"/select,\"" + path + L"\"").c_str(), NULL, SW_SHOWNORMAL);
    }

    void MainWindow::OpenHistoryFile(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::ItemClickEventArgs const& e)
    {
        if (auto item{ e.ClickedItem().try_as<ImageMerger::HistoryItem>() })
        {
            ShellExecute(NULL, NULL, item.HistoryPath().c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    }
    #pragma endregion
}
