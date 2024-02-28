#pragma once

#include "MainWindow.g.h"
#include "ImageItem.g.h"
#include "HistoryItem.g.h"
#include "Utilities.h"
#include "winrt/Microsoft.UI.Composition.SystemBackdrops.h"
#include "winrt/Windows.System.h"
#include <dispatcherqueue.h>
#include "../../../Libraries/xellanix.objects.h"
#include "winrt/Microsoft.UI.Xaml.Media.Imaging.h"
#include "winrt/Windows.Storage.Pickers.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/Windows.ApplicationModel.DataTransfer.h"
#include "winrt/Windows.Graphics.Imaging.h"
#include "winrt/Microsoft.UI.Xaml.Documents.h"
#include "winrt/Microsoft.UI.Text.h"
#include "..\..\..\Libraries\arith_range.h"
#include "winrt/Microsoft.UI.Xaml.Media.h"

namespace winrt
{
    namespace MUC = Microsoft::UI::Composition;
    namespace MUCSB = Microsoft::UI::Composition::SystemBackdrops;
    namespace MUX = Microsoft::UI::Xaml;
    namespace WS = Windows::System;
}

namespace winrt::ImageMerger::implementation
{
    struct ImageItem : ImageItemT<ImageItem>
    {
    private:
        hstring m_itemName{ L"" };
        hstring m_itemPath{ L"" };

    public:
        ImageItem() = default;

        ImageItem(hstring const& itemName, hstring const& itemPath) : m_itemName(itemName), m_itemPath(itemPath)
        {
        }

        hstring ItemName() { return m_itemName; }
        hstring ItemPath() { return m_itemPath; }

        void ItemName(hstring const& value)
        {
            m_itemName = value;
        }
        void ItemPath(hstring const& value)
        {
            m_itemPath = value;
        }
    };

    struct HistoryItem : HistoryItemT<HistoryItem>
    {
    private:
        hstring m_historyName{ L"" };
        hstring m_historyPath{ L"" };
        hstring m_historySize{ L"" };
        hstring m_historyDate{ L"" };

    public:
        HistoryItem() = default;

        HistoryItem(hstring const& historyName, hstring const& historyPath,
                    hstring const& historySize, hstring const& historyDate) :
            m_historyName(historyName), m_historyPath(historyPath), m_historySize(historySize),
            m_historyDate(historyDate)
        {}

        hstring HistoryName() { return m_historyName; }
        hstring HistoryPath() { return m_historyPath; }
        hstring HistorySize() { return m_historySize; }
        hstring HistoryDate() { return m_historyDate; }

        void HistoryName(hstring const& value)
        {
            m_historyName = value;
        }
        void HistoryPath(hstring const& value)
        {
            m_historyPath = value;
        }
        void HistorySize(hstring const& value)
        {
            // 0.00MB
            m_historySize = value;
        }
        void HistoryDate(hstring const& value)
        {
            // Fri, December 9, 2022 05:58 PM
            m_historyDate = value;
        }
    };

    struct MainWindow : MainWindowT<MainWindow>
    {
    private:
        #pragma region Mica Material Background
        winrt::MUCSB::SystemBackdropConfiguration m_configuration{ nullptr };
        winrt::MUCSB::MicaController m_backdropController{ nullptr };
        winrt::MUX::Window::Activated_revoker m_activatedRevoker;
        winrt::MUX::Window::Closed_revoker m_closedRevoker;
        winrt::MUX::FrameworkElement::ActualThemeChanged_revoker m_themeChangedRevoker;
        winrt::MUX::FrameworkElement m_rootElement{ nullptr };
        winrt::WS::DispatcherQueueController m_dispatcherQueueController{ nullptr };
        
        void SetBackground();
        winrt::WS::DispatcherQueueController CreateSystemDispatcherQueueController();
        void SetupSystemBackdropConfiguration();
        winrt::MUCSB::SystemBackdropTheme ConvertToSystemBackdropTheme(winrt::MUX::ElementTheme const& theme);
        void SetSystemButtonTheme(bool isDarkMode);
        #pragma endregion
        
        #pragma region Modern Title Bar
        void SetModernAppTitleBar();
        #pragma endregion

        #pragma region Image List Properties
        using InspectableVector = Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable>;
        InspectableVector m_imageItems;
        #pragma endregion

        #pragma region History List Properties
        InspectableVector m_historyItems;
        #pragma endregion

        #pragma region Utilities
        std::wstring get_filename(std::wstring path)
        {
            if (auto pos = path.find_last_of(L"\\/"); pos != std::wstring::npos)
                path = path.substr(pos + 1);

            return path;
        }

        std::wstring GetStringByLine(std::wstring const& data, uint64_t index)
        {
            std::wistringstream wss(data);
            for (uint64_t i = 0; i < index; i++)
            {
                wss.ignore((std::numeric_limits<std::streamsize>::max)(), L'\n');
            }
            std::wstring line;
            std::getline(wss, line);

            return line;
        }
        Windows::Foundation::IAsyncAction RemoveHistoryData(std::wstring const& path);

        size_t findCaseInsensitive(std::wstring data, std::wstring toSearch, size_t pos = 0)
        {
            // Convert complete given String to lower case
            std::transform(data.begin(), data.end(), data.begin(), ::towlower);
            // Convert complete given Sub String to lower case
            std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::towlower);
            // Find sub string in given string
            return data.find(toSearch, pos);
        }

        void removeline(std::wstring& data, std::wstring const& toRemove, size_t pos)
        {
            if (pos == 0)
            {
                if (data.size() > toRemove.size())
                {
                    if (data.at(toRemove.size()) == L'\n')
                    {
                        data.erase(pos, toRemove.length() + 1);
                        return;
                    }
                }
            }
            else
            {
                data.erase(pos - 1, toRemove.length() + 1);
                return;
            }

            data.erase(pos, toRemove.length());
        }
        bool GetHistoryData(std::wstring& data)
        {
            auto historypath = Xellanix::Utilities::GetLocalAppData(L"Xellanix\\ImageMerger") / L"history.xihd";
            if (Xellanix::Utilities::CheckExist(historypath))
            {
                std::wifstream wif;
                wif.open(historypath, std::ios::binary);
                std::wostringstream wis;
                wis << wif.rdbuf();
                data = wis.str();
                wif.close();

                return true;
            }
            return false;
        }
        void MakeHistoryData(std::wstring const& data)
        {
            auto historypath = Xellanix::Utilities::GetLocalAppData(L"Xellanix\\ImageMerger") / L"history.xihd";

            fs::create_directories(fs::path(historypath).parent_path());

            std::wofstream wof;
            wof.open(historypath, std::ios::binary);
            wof << data;
            wof.close();
        }
        void CheckAndAppend(std::wstring const& path)
        {
            if (std::wstring data; GetHistoryData(data))
            {
                if (!data.empty())
                {
                    std::wofstream wof;
                    wof.open(Xellanix::Utilities::GetLocalAppData(L"Xellanix\\ImageMerger") / L"history.xihd", std::ios::binary);

                    if (auto pos = findCaseInsensitive(data, path); pos != std::wstring::npos)
                    {
                        removeline(data, path, pos);
                    }

                    if (!data.empty())
                    {
                        wof << data << std::endl << path;
                        wof.close();

                        return;
                    }
                }
            }
            
            // Not Exist or Null
            MakeHistoryData(path);
        }

        template<typename T>
        fire_and_forget no_await(T t)
        {
            if constexpr (std::is_invocable_v<T>)
            {
                co_await t();
            }
            else
            {
                co_await t;
            }
        }

        template<typename T>
        void try_cancel(T& info)
        {
            if (info && info.Status() == Windows::Foundation::AsyncStatus::Started) info.Cancel();
        }
        #pragma endregion

        #pragma region Revokers
        InspectableVector::VectorChanged_revoker vectorToken{};
        Microsoft::UI::Xaml::DispatcherTimer::Tick_revoker tick_revoker{}, fileChecker_revoker{};

        Microsoft::UI::Xaml::DispatcherTimer checkProcess{}, fileChecker{};
        #pragma endregion

        #pragma region Setting Properties
        int32_t m_source = 0;
        std::wstring defaultType{ L".pdf" };
        std::time_t last_op_t = time(0);
        std::wstring last_op = L"";
        std::wstring last_file_path = L"";
        std::wstring _expansionMode = L"vertical";
        bool _useCompression = false;
        bool _autoSelect = true;
        bool _autoOpen = true;
        bool _autoReveal = false;
        bool _autoClose = false;
        unsigned short _compressionIntensity = 25;
        double _maxHistory = 10.0;
        #pragma endregion

        #pragma region Common Properties
        bool isloaded = false;
        time_t last_ctime1 = 0, last_mtime1 = 0;
        long long last_size1 = 0;
        Xellanix::Objects::XSMF settings{ L"Image Merger" };

        PROCESS_INFORMATION pi = {};

        Windows::Foundation::IAsyncAction reloadHistoryOp{ nullptr };
        #pragma endregion

        Windows::Foundation::IAsyncAction ProcessActivationInfo();
        void FromFile(param::hstring const& filepath);
        Windows::Foundation::IAsyncAction ReloadHistory(std::wstring const& data);
        
        template <typename ancestor_type>
        auto find_ancestor(::winrt::Microsoft::UI::Xaml::DependencyObject root) noexcept
        {
            auto ancestor{ root.try_as<ancestor_type>() };
            while (!ancestor && root)
            {
                root = ::winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(root);
                ancestor = root.try_as<ancestor_type>();
            }
            return ancestor;
        }

        #pragma region Setting Manager
        void LoadSettings();
        template<typename T>
        typename std::enable_if<std::bool_constant<std::_Is_any_of_v<std::remove_cv_t<T>, bool, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, std::wstring>>::value>::type
            SaveSettings(std::wstring const& name, T const& value);
        #pragma endregion

    public:
        MainWindow();

        InspectableVector ImageItems() { return m_imageItems; }
        void ImageItems(InspectableVector const& value)
        {
            if (m_imageItems != value)
            {
                m_imageItems = value;
            }
        }
        
        InspectableVector HistoryItems() { return m_historyItems; }
        void HistoryItems(InspectableVector const& value)
        {
            if (m_historyItems != value)
            {
                m_historyItems = value;
            }
        }

        void Grid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void Window_Closed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::WindowEventArgs const& args);
        
        // InfoBar Events
        void OpenResultClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // ImageList Events
        void ImageSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void FilesDragOver(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e);
        Windows::Foundation::IAsyncAction FilesDrop(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e);
        
        // ControlBar Events
        Windows::Foundation::IAsyncAction AddClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void DeleteItemClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void ClearItemClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void SelectAllClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void DeselectClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        Windows::Foundation::IAsyncAction MergeItemClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Merging Option Events
        void SettingsSourceChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void DefaultExtensionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void UseCompressionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void IntensityChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
        void ExpansionModeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void AutoSelectChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void AutoOpenChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void AutoRevealChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void AutoCloseChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // History Option Events
        void MaxHistoryChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);

        // History Items Events
        Windows::Foundation::IAsyncAction RemoveHistory(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void ShowContextMenu(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        Windows::Foundation::IAsyncAction DeleteByClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void RevealMergedFile(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        Windows::Foundation::IAsyncAction PivotSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void OpenHistoryFile(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::ItemClickEventArgs const& e);
};
}

namespace winrt::ImageMerger::factory_implementation
{
    struct ImageItem : ImageItemT<ImageItem, implementation::ImageItem>
    {
    };

    struct HistoryItem : HistoryItemT<HistoryItem, implementation::HistoryItem>
    {};

    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
