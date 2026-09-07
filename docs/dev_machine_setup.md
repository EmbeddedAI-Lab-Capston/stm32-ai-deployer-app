# Yeni Makine Kurulum Tuzakları

> Bu doküman **git'e commit edilir** — memory'nin aksine makine değişince
> kaybolmaz. Amaç: 2026-09-07'de bu projeyi sıfır bir Windows makinede
> ayağa kaldırırken karşılaşılan, **her yeni makinede tekrar çıkma
> ihtimali olan** sorunları tek yerde toplamak. Kodda kalıcı olarak
> düzeltilmiş hatalar burada YOK — onlar zaten repoyla birlikte geliyor,
> hangi makineye klonlarsan klonla otomatik düzelmiş oluyor.
>
> Kısa kural: bir sorun "kodu değiştirerek" çözüldüyse buraya girmez, sadece
> "bu makineye özel bir ayar/kurulum yaparak" çözüldüyse buraya girer.

---

## 1. Proje yolu ASCII olmayan karakter içeriyorsa `qmlimportscanner` çöker

**Belirti:** `cmake --build` sırasında
`qmlimportscanner: No such file or directory: "<proje yolu>"` hatası,
CMake configure/build burada durur.

**Sebep:** Qt'nin bazı Windows araçları (qmlimportscanner ve — bkz. madde 3 —
`stedgeai`) proje yolundaki Türkçe/Latin-olmayan karakterleri (`ı`, `ğ`, `ş`
gibi) doğru işleyemiyor. Bu proje `Yazılım` gibi bir klasör adı altında
tutuluyorsa (veya Windows kullanıcı adında/klasör isminde benzer bir karakter
varsa) sorun her yeni makinede tekrar çıkar.

**Kalıcı çözüm yok, geçici çözüm (her makinede tekrar uygulanmalı):**
Projeyi ASCII bir yola bağlayan bir junction oluştur ve **her zaman o
junction üzerinden** build al:

```powershell
cmd /c mklink /J "C:\dev\stm32-ai-deployer-app" "<gerçek proje yolu>"
cd C:\dev\stm32-ai-deployer-app
cmake --build build -j
```

En temiz çözüm aslında projeyi baştan ASCII bir yola klonlamak
(`C:\dev\stm32-ai-deployer-app` gibi) — junction'a hiç gerek kalmaz.

---

## 2. `aqtinstall`'ın PyPI sürümü Qt 6.10+ kuramıyor

**Belirti:** `pip install aqtinstall` sonrası `aqt install-qt ...` veya
`aqt list-qt ... --arch` gibi komutlar
`Failed to download checksum for the file 'Updates.xml'` hatası verir.

**Sebep:** Qt, 6.10 sürümünden itibaren download.qt.io'daki depo klasör
yapısını değiştirdi; PyPI'daki `aqtinstall` (bu yazı itibarıyla v3.3.0) bu
yeni yapıyı henüz desteklemiyor. Bu, aqtinstall'ın kendi upstream sürümüyle
ilgili bir kısıtlama — proje kodundan bağımsız, yeni bir PyPI sürümü
çıkana kadar her makinede tekrar çıkar.

**Çözüm (her makinede tekrar uygulanmalı, PyPI güncellenene kadar):**
```powershell
pip install --upgrade "git+https://github.com/miurahr/aqtinstall.git"
```
GitHub'ın `main` dalı yeni depo yapısını destekliyor. İleride
`pip install --upgrade aqtinstall` (normal PyPI) çalışıyorsa artık bu
adıma gerek kalmamış demektir — önce onu dene.

---

## 3. Eski ST-Link (V2-1, örn. F4 Nucleo) probunun debug arayüzü sürücüsüz kalır

**Belirti:** Aygıt Yöneticisi'nde F4'ün ST-Link'ine ait "ST-Link Debug"
composite arayüzü (MI_00) sürücü hatası ("Error") gösterir. UART/VCP
(COM port) çalışır ama flash/GDB/Register Inspector/Değişken İzleyici gibi
ST-Link üzerinden debug gerektiren özellikler F4'te çalışmaz.

**Sebep:** H7/N6'daki daha yeni ST-Link V3/V3E arayüzleri Windows'un yerleşik
WinUSB sürücüsüyle çalışıyor; F4'ün eski ST-Link/V2-1'i ise
STMicroelectronics'in kendi USB sürücüsüne ihtiyaç duyuyor. Bu sürücü
Windows'ta hazır gelmiyor.

**Çözüm:** STM32CubeIDE veya standalone STM32CubeProgrammer kurulumu
sırasında ST-Link sürücüsünü de kur (installer genelde soruyor). Kurulum
sonrası F4'ü çıkarıp takmak (ya da makineyi yeniden başlatmak) gerekebilir.

---

## 4. `ctest` için Qt'nin gerçek `bin` klasörü PATH'te olmalı, sadece `Tools\` yetmez

**Belirti:** `ctest --test-dir build` çalıştırıldığında
`STM32AiDeployerTests` `0xc0000135` (DLL bulunamadı) hatasıyla "başarısız"
görünür — ama gerçekte test kodunda hata yoktur.

**Sebep:** `windeployqt` yalnızca ana `STM32AiDeployer` hedefine karşı
çalışıyor (CMakeLists.txt'teki POST_BUILD adımı); ayrı test executable'ı
(`STM32AiDeployerTests.exe`) için Qt DLL'leri (özellikle `Qt6Test.dll`)
build klasörüne hiç kopyalanmıyor.

**Çözüm:** `ctest` çalıştırmadan önce Qt'nin gerçek kurulum `bin`
klasörünü de PATH'e ekle (yalnızca `Tools\CMake_64` ve `Tools\mingw1310_64`
yetmez):

```powershell
$env:PATH = "<QT_ROOT>\<versiyon>\mingw_64\bin;<QT_ROOT>\Tools\CMake_64\bin;<QT_ROOT>\Tools\mingw1310_64\bin;$env:PATH"
ctest --test-dir build --output-on-failure
```

---

## 5. ST kurulum dosyaları tarayıcıdan hesapla indirilmeli, otomatikleştirilemez

**Belirti/durum:** STM32CubeIDE ve standalone `stedgeai` (ST Edge AI Core)
indirmeleri ST'nin sitesinde ücretsiz bir myST hesabı girişi istiyor.

**Neden buraya giriyor:** Bu adım bir AI asistanının (kimlik bilgisi
paylaşılmadığı sürece) otomatikleştiremeyeceği tek adım — her yeni makinede
elle: hesaba giriş yapıp indirmek, sonra installer'ı çalıştırmak gerekiyor.
İndirme bitince kurulum/konfigürasyon adımları (yol tespiti, `--c-api legacy`
gibi uyumluluk bayrakları vb.) otomatik/asistan yardımıyla yapılabilir.

**Öneri:** İki dosyayı (STM32CubeIDE installer, `stedgeai-win.zip`) bir
bulut depoda (Drive/OneDrive) veya ekip içi paylaşılan bir klasörde tutmak,
her yeni makinede tekrar ST hesabına girip aramaktan daha hızlı olur.

---

## 6. ST araçlarının sürümü zamanla ilerliyor — X-CUBE-AI/stedgeai uyumluluğu tekrar kırılabilir

**Durum:** Bu proje `X-CUBE-AI 10.2.0` (~`ST Edge AI Core v2.2.0`) ile test
edilmiş şablonlar içeriyor. 2026-09-07'de bu makineye kurulan
`ST Edge AI Core v4.0.1` (`STM32CubeAI 12.0.1`) ile aradaki 2 majör versiyon
farkı `XCubeAIRunner`'a eklenen `--c-api legacy` bayrağıyla köprülendi (bkz.
`src/modules/flash/XCubeAIRunner.cpp` içindeki yorumlar) — bu **kod
düzeyinde kalıcı bir çözüm**, tekrar yapılmasına gerek yok.

**Ama şu risk kalıcı değil:** ST bir sonraki sürümde `--c-api legacy`'yi
kaldırırsa veya başka bir API değişikliği yaparsa, aynı sınıf sorun tekrar
çıkabilir. Böyle bir şey olursa doğrulama yöntemi hazır:

```powershell
stedgeai.exe generate --model <bir .tflite> --target stm32 --output <tmp> `
  --quiet --workspace <ascii tmp yol> --c-api legacy
```
çıktısındaki `network.h`'yi `templates/ai_glue/ai_config.h`'nin beklediği
`AI_NETWORK_IN_1_SIZE` / `AI_NETWORK_OUT_1_SIZE` /
`AI_NETWORK_DATA_ACTIVATIONS_SIZE` makrolarıyla karşılaştır.

---

## Buraya girmeyenler (kod düzeyinde kalıcı olarak çözüldü, tekrar not almaya gerek yok)

- `Backend::scanTools()`'taki `Qt::UniqueConnection` + lambda hatası (gcc/make/
  stedgeai hiç otomatik bulunamıyordu) — düzeltildi, repoyla gelir.
- `ToolDetector::detectXCubeAI()`'nin standalone `stedgeai` kurulum yolunu
  hiç aramaması — düzeltildi, repoyla gelir.
- `stedgeai`'nin interaktif "istatistik izni" sorusu (QProcess'i sonsuza kadar
  bekletebilirdi) — `--quiet` bayrağı koda gömüldü, repoyla gelir.
- `stedgeai`'nin Türkçe yol içeren `workspace dir`'i loglarken çökmesi —
  `--workspace <ascii tmp>` koda gömüldü, repoyla gelir (madde 1'deki
  `qmlimportscanner` sorunuyla karıştırılmasın — o hâlâ manuel junction ister).
