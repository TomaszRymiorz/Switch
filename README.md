# Switch 2
Oprogramowanie włącznika światła automatycznego domu. 

### Włącznik
Całość zbudowana na WIFI D1 mini z modułem Data logger (RTC DS1307 + MicroSD), APDS-9960 oraz 2-kanałowym przekaźnikiem SSR.

### Możliwości
Łączność z włącznikiem odbywa się przez sieć Wi-Fi.
Dane dostępowe do routera przechowywane są wraz z innymi informacjami na karcie pamięci. Pozwala to uniknąć przykrych niespodzianek po zaniku prądu i daje możliwość szybkiego kopiowania ustawień między urządzeniami.
W przypadku braku informacji o sieci, urządzenie aktywuje wyszukiwania routera z wykorzystaniem funkcji WPS.
Włącznik można ustawić w tryb online, wówczas będzie on sprawdzał dedykowany Webservice i możliwe stanie się sterowanie przez Internet.

Włącznik automatycznie łączy się z zaprogramowaną siecią Wi-Fi w przypadku utraty połączenia.

Zawiera czujnik gestów wykorzystywany do sterowania oświetleniem bez udziału dedykowanej aplikacji. Gestem można również opuścić lub podnieść rolety. Dane z czujnika wysyłane są do bliźniaczego urządzenia w pomieszczeniu jakim jest roleta. Parowanie urządzeń odbywa się poprzez aplikację dedykowaną.

Włącznik wykorzystuje RTC do wywoływania ustawień automatycznych.
Ustawienia automatyczne obejmują włączanie i wyłączanie światła o wybranej godzinie oraz włączanie po zapadnięciu zmroku i wyłączanie o świcie. Powtarzalność obejmuje okres jednego tygodnia, a ustawienia nie są ograniczone ilościowo. W celu zminimalizowania objętości wykorzystany został zapis tożsamy ze zmienną boolean, czyli dopiero wystąpienie znaku wskazuje na włączoną funkcję.

* '4' wszystkie światła, którymi steruje włącznik - występuje tylko w zapisie aplikacji w celu zminimalizowania ilości przesyłanych danych
* '1', '2', '3' numer światła, którym steruje włącznik
* 'w' cały tydzień - występuje tylko w zapisie aplikacji w celu zminimalizowania ilości przesyłanych danych
* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* 'n' włącz po zmroku / wyłącz o świcie
* '_' włącz o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* '-' wyłącz o godzinie - jeśli występuje w zapisie, po nim znajduje się godzina w zapisie czasu uniksowego

Przykład zapisu trzech ustawień automatycznych: 1140_12w-420,4asn,1ouehrn-300

### Sterowanie
Sterowanie włącznikiem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy włącznika.

* "/set" - Pod ten adres przesyłane są ustawienia dla włącznika, dane przesyłane w formacie JSON. Ustawić można strefę czasową ("offset"), czas RTC ("access"), IP bliźniaczego urządzenia ("twin"), ustawienia automatyzujące ("smart"), włączyć lub wyłączyć światła ("light").

* "/state" - Służy do regularnego odpytywania włącznika o jego podstawowy stan, włączone światła ("light").
