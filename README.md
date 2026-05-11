# Tarsau Archive System

C dilinde geliştirilmiş, sıkıştırma yapmadan çalışan basit bir arşivleme sistemi.

## Proje Amacı

Bu proje, Linux/Unix ortamında çalışan ve metin dosyalarını `.sau` uzantılı özel bir arşiv formatında birleştiren bir arşivleme programıdır.

Program:

- ASCII metin dosyalarını arşivler
- Dosya izinlerini korur
- Arşiv dosyasını tekrar açabilir
- Bozuk arşivleri tespit eder
- 200 MB ve 32 dosya sınırı uygular

---

# Derleme

```bash
gcc -Wall -Wextra -std=c11 -o tarsau tarsau.c
```

---

# Kullanım

## Arşiv oluşturma

```bash
./tarsau -b file1.txt file2.txt -o arsiv.sau
```

Varsayılan çıktı:

```bash
./tarsau -b file1.txt file2.txt
```

Bu durumda çıktı:

```text
a.sau
```

---

## Arşiv açma

```bash
./tarsau -a arsiv.sau
```

Belirli dizine açma:

```bash
./tarsau -a arsiv.sau output_directory
```

---

# SAU Arşiv Formatı

Arşiv dosyası iki bölümden oluşur:

## 1. Organizasyon Bölümü

İlk 10 byte:

- metadata boyutu
- ASCII sayısal format

Dosya kayıt formatı:

```text
|dosya_adi,izinler,boyut|
```

Örnek:

```text
|test.txt,644,120|
```

---

## 2. Dosya İçerikleri

Dosya içerikleri:

- ayraç kullanılmadan
- art arda
- ASCII formatta

saklanır.

---

# Hata Kontrolleri

Program aşağıdaki durumları kontrol eder:

- ASCII dışı dosya kontrolü
- Maksimum 32 dosya
- Maksimum 200 MB toplam boyut
- Bozuk `.sau` arşivi
- Geçersiz parametreler
- Dosya erişim hataları

---

# Kullanılan Sistem Fonksiyonları

- fopen
- fread
- fwrite
- fseek
- stat
- chmod
- mkdir

---

# Geliştirici

Rafet DUR
