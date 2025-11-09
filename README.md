AI_Smart_Surveillance_System/
│
├── CMakeLists.txt
├── README.md
├── assets/                     ← необов’язкове: raw зображення, моделі
│   ├── models/                 ← ONNX-файли (FaceNet, YOLO)
│   ├── test_videos/
│   └── fonts/
│
├── src/
│   ├── main.cpp
│   │
│   ├── core/                   ← основна логіка / ядро
│   │   ├── CameraManager.h/.cpp        ← робота з декількома камерами
│   │   ├── AIProcessor.h/.cpp          ← обробка кадрів, FaceNet, DNN
│   │   ├── HeatmapAccumulator.h/.cpp   ← накопичення "тепла"
│   │   ├── EventLogger.h/.cpp          ← логи AI / система / події
│   │   ├── SupabaseClient.h/.cpp       ← REST API до сервера Drogon/Supabase
│   │   ├── ThreadPool.h/.cpp           ← пул потоків для камер / AI
│   │   └── SettingsManager.h/.cpp      ← конфігурації (QSettings / JSON)
│   │
│   ├── ui/                     ← інтерфейс (вікна, сторінки, панелі)
│   │   ├── MainWindow.h/.cpp          ← головне вікно (QMainWindow)
│   │   ├── LoginWindow.h/.cpp         ← екран входу
│   │   ├── pages/
│   │   │   ├── DashboardPage.h/.cpp
│   │   │   ├── CamerasPage.h/.cpp
│   │   │   ├── HeatmapPage.h/.cpp
│   │   │   ├── EventsPage.h/.cpp
│   │   │   ├── AnalyticsPage.h/.cpp
│   │   │   ├── Face3DPage.h/.cpp
│   │   │   └── SettingsPage.h/.cpp
│   │   ├── widgets/
│   │   │   ├── CameraTile.h/.cpp       ← окремий віджет для однієї камери
│   │   │   ├── ConsoleWidget.h/.cpp    ← консоль знизу
│   │   │   ├── SidebarWidget.h/.cpp    ← ліва панель (режими)
│   │   │   ├── StatusBarWidget.h/.cpp  ← нижня смужка статусу
│   │   │   └── PopupNotification.h/.cpp← спливаючі повідомлення
│   │   └── dialogs/
│   │       ├── CameraConfigDialog.h/.cpp
│   │       └── AboutDialog.h/.cpp
│   │
│   ├── qss/                    ← стилі оформлення (Qt Style Sheets)
│   │   ├── dark.qss
│   │   ├── light.qss
│   │   └── variables.qss       ← кольорові константи
│   │
│   ├── icons/                  ← SVG або PNG іконки для sidebar, toolbar
│   │   ├── dashboard.svg
│   │   ├── camera.svg
│   │   ├── heatmap.svg
│   │   ├── logs.svg
│   │   ├── analytics.svg
│   │   ├── face3d.svg
│   │   ├── console.svg
│   │   └── settings.svg
│   │
│   ├── resources.qrc           ← реєстрація ресурсів (QSS, іконки, шрифти)
│   │
│   └── utils/                  ← дрібні утиліти (формати, конвертації)
│       ├── ImageUtils.h/.cpp
│       ├── JsonUtils.h/.cpp
│       └── TimeUtils.h/.cpp
│
├── tests/                      ← GTest / QtTest
│   ├── test_ai.cpp
│   ├── test_heatmap.cpp
│   ├── test_camera.cpp
│   └── CMakeLists.txt
│
└── config/
    ├── appsettings.json        ← сервер, шляхи, FPS, логування
    ├── supabase.json           ← ключі API (не комітити)
    └── cameras.json            ← список камер (url, id, name)


    Опис головних директорій
| Каталог     | Призначення                                                 |
| ----------- | ----------------------------------------------------------- |
| **core/**   | Уся бізнес-логіка: AI, камери, мережа, база, налаштування   |
| **ui/**     | Класи інтерфейсу (головне вікно, сторінки, панелі, діалоги) |
| **qss/**    | Теми оформлення (dark/light)                                |
| **icons/**  | Іконки SVG/PNG, підключені через `.qrc`                     |
| **utils/**  | Утилітарні функції (конвертація JSON, часу, зображень)      |
| **tests/**  | Модульні тести (GoogleTest або QtTest)                      |
| **config/** | Конфігураційні JSON-файли для серверів, камер тощо          |
| **assets/** | AI-моделі (ONNX), тестові відео, шрифти, інше               |



| Модуль            | Виконує                                                   |
| ----------------- | --------------------------------------------------------- |
| `MainWindow`      | збирає `SidebarWidget`, `ConsoleWidget`, `QStackedWidget` |
| `SidebarWidget`   | список режимів (Dashboard, Cameras, Heatmap, ...)         |
| `CamerasPage`     | показує 2×2 / 3×3 потоки                                  |
| `HeatmapPage`     | накладає Heatmap поверх відео                             |
| `AIProcessor`     | обробляє кадри → embeddings / faces                       |
| `SupabaseClient`  | відправляє дані на сервер (Drogon / Supabase)             |
| `EventLogger`     | пише логи у консоль і файл                                |
| `ConsoleWidget`   | відображає лог на екрані                                  |
| `SettingsManager` | читає config JSON і керує QSettings                       |


Кожен модуль = окремий .h/.cpp

Не тримай AI-код усередині UI-класів.

core = вся “мозкова” логіка без QWidget.

ui = тільки візуальна частина.

QSS — підключається в main.cpp або в MainWindow.

Іконки SVG — кладеш у icons/ і підключаєш через ":/icons/name.svg".

Все, що не відображається — поза ui.