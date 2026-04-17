# USB FAT32 Creator

Desktopowa aplikacja C++/Qt do bezpiecznego planowania i przygotowania podziału fizycznych nośników USB.

Główna idea:
- tworzenie partycji FAT32 (maks. 32 GB na partycję),
- wykorzystanie pozostałej przestrzeni jako exFAT,
- opcjonalny podział na wiele "wirtualnych penów" FAT32.

## Bezpieczeństwo

To narzędzie operuje na fizycznych dyskach. Traktuj je jak narzędzie administracyjne.

- Domyślnie aplikacja działa w trybie `dry-run`.
- Operacje partycjonowania i formatowania usuwają dane.
- Przed wykonaniem pojawia się wyraźne ostrzeżenie o nieodwracalności zmian.
- Zalecany workflow: `Testuj` -> `Zbuduj plan` -> `Wykonaj`.
- Wymagane są uprawnienia administratora/root.

## Najważniejsze funkcje

- Wykrywanie fizycznych urządzeń (bez partycji logicznych).
- Budowanie planu podziału FAT32 + exFAT.
- Symulacja planu (`Testuj`) bez zmian na dysku.
- Walidacja planu przed wykonaniem (m.in. limity FAT32, spójność rozmiarów, reguły exFAT).
- Generowanie listy poleceń systemowych dla macOS/Linux/Windows.

## Wymagania

- CMake 3.21+
- Kompilator C++17
- Qt6 Widgets

## Pobieranie

Gotowe wersje portable są publikowane w GitHub Releases:

- Windows: ZIP z `usb_fat32_creator.exe` i bibliotekami Qt
- macOS: ZIP z `USB FAT32 Creator.app`
- Linux: ZIP z binarką i skryptem `run.sh`

Pobieranie:

- Releases: https://github.com/pnieradko98/usbFAT32Creator/releases
- Najnowsze wydanie: https://github.com/pnieradko98/usbFAT32Creator/releases/latest

Aktualne paczki z release `v1.0.4`:

- Windows: `usb_fat32_creator_windows_portable.zip`
- macOS: `usb_fat32_creator_macos_portable.zip`
- Linux: `usb_fat32_creator_linux_portable.zip`

## Szybki start

### macOS / Linux

```bash
cmake -S . -B build
cmake --build build -j
./build/usb_fat32_creator
```

### Windows (PowerShell)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\usb_fat32_creator.exe
```

## Portable EXE (Windows)

### Opcja A: automatycznie przez GitHub Actions

Workflow: `.github/workflows/windows-portable.yml`

Co robi pipeline:
- buduje wersję `Release`,
- uruchamia `windeployqt` (dołącza zależności Qt),
- pakuje aplikację do ZIP,
- publikuje artifact `usb-fat32-creator-windows-portable`.

Dla tagów `v*` (np. `v1.0.2`) ten sam ZIP trafia także do GitHub Releases.

## Gotowe ZIP-y dla macOS i Linux

Workflow: `.github/workflows/unix-zips.yml`

Pipeline buduje i publikuje:
- `usb-fat32-creator-macos-portable` (artifact ZIP z aplikacją `.app`),
- `usb-fat32-creator-linux-portable` (artifact ZIP z binarką i skryptem startowym).

Dla tagów `v*` oba ZIP-y trafiają także do GitHub Releases jako assety.

### Jak pobrać gotowe paczki

1. Wejdź w zakładkę Actions.
2. Uruchom workflow `unix-zips-build`.
3. Po zakończeniu pobierz artifacty:
	- `usb-fat32-creator-macos-portable`,
	- `usb-fat32-creator-linux-portable`.

### Uwaga o Linux

Paczka Linux zawiera gotową binarkę i skrypt uruchomieniowy, ale na części dystrybucji może wymagać obecnego systemowego runtime Qt6.

### Opcja B: ręcznie na Windows

Po zbudowaniu `Release` dołącz zależności Qt:

```powershell
$exe = ".\build\Release\usb_fat32_creator.exe"
windeployqt --release --compiler-runtime --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
```

Następnie spakuj katalog z EXE i bibliotekami do ZIP.

## Jak działa aplikacja

1. Wybierasz fizyczne urządzenie.
2. Ustawiasz liczbę partycji FAT32 (lub auto).
3. Klikasz `Testuj`, aby zobaczyć symulację i przejść walidację.
4. Klikasz `Zbuduj plan`, aby wygenerować komendy.
5. Klikasz `Wykonaj` tylko po świadomej akceptacji ryzyka.

## Ograniczenia i uwagi

- Aplikacja jest multiplatformowa, ale binarki budujesz natywnie na danym systemie.
- Na Windows ścieżka wykonania może wymagać ręcznego użycia planu `diskpart` (zależnie od konfiguracji środowiska i uprawnień).
- Zawsze testuj na nośniku nieprodukcyjnym, zanim użyjesz narzędzia operacyjnie.
