m4_include(`version.m4i')m4_dnl
Wtyczka: Integrity Checker
Wersja: PLUGIN_VERSION
Autor: Ariman
URL: https://github.com/lazyhamster/IntChecker

--------------------------------------------------------

1. Opis.
Wtyczka została zaprojektowana do generowania sum kontrolnych w różnych
algorytmach i sprawdzania integralności plików z wcześniej utworzonymi
sumami kontrolnymi.

Obsługiwane są nieograniczone poziomy zagnieżdżenia plików w folderach.

Zaimplementowane algorytmy:
CRC32, MD5, SHA1, SHA-256, SHA-512, SHA3-512, Whirlpool

Wtyczka bazuje na kodzie programu RHash (https://github.com/rhash/RHash).

2. Wymagania.
Minimalna wersja FAR do pracy z wtyczką to 3.0.5886

3. Gwarancja.
Wtyczka jest udostępniana "jak jest" (bez gwarancji). Autor nie odpowiada
za konsekwencje wynikające z używania tego programu.

4. Licencja.
Wtyczka dostarczana jest na licencje GNU GPL v3.
Tekst licencji (po angielsku) można znaleźć w folderze wtyczki.

4. Praca z wtyczką.
Wtyczkę można wywołać na kilka sposobów:
- poprzez listę wtyczek (klawisz F11)
- poprzez przedrostek (niestandardowo, do dalszego sprawdzenia)

Działa tylko z panelami plików. Nie ma reakcji na próbę wywołania wtyczki z dowolnego innego panelu.

4.1. Generowanie sum kontrolnych.
Algorytm można wybrać w oknie dialogowym wtyczki.
Domyślny algorytm można wybrać w menu konfiguracji wtyczki.
Wtyczka tworzy 2 rodzaje plików: .sfv (dla sum CRC32) i pliki podobne do md5,
obsługiwane m.in. przez programy md5sum, sha256sum, itp.
W trybie odczytu (weryfikacja) wtyczka rozpoznaje pliki w formacie BSD.
Można utworzyć jeden wspólny plik kontrolny dla wszystkich plików lub osobne pliki dla każdego
sprawdzanego pliku.

4.2. Weryfikacja sum.
Jeśli w panelu zostanie wybrany jeden plik, do menu zostanie dodana nowa pozycja do sprawdzania plików.
Jeśli plik zostanie pomyślnie rozpoznany, rozpocznie się weryfikacja pliku.
Możliwe jest także porównanie skrótu pojedynczego pliku z zawartością schowka.

4.3. Porównywanie plików z różnych paneli.
Dodatkową funkcjonalnością jest możliwość porównania plików o takich samych nazwach
w różnych panelach plików. Porównanie odbywa się zgodnie z wybranym algorytmem
w oknie dialogowym wtyczki. Jeśli ten sam katalog jest wybrany na obu panelach, lub
drugi panel należy do innej wtyczki, funkcja ta będzie niedostępna.

5. Kontakty.
Komunikaty o błędach, sugestie, pomysły, itp. wyślij na adres ariman@inbox.ru
lub przez system GitHub.
