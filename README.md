# Tabord' RGB

Устройство на базе EPS8266 EPS-12F с автономным Wi-Fi модулем, предназначенное для отображения (индикации) текущей боевой ситуации альянса.

Устройство изготавливалось кустарным способом в индивидуальном порядке, и в единственном (пока) варианте для [альянса GF Company](https://zkillboard.com/alliance/99008697/) игры EVE Online. Проект получил своё название по имени одного из игроков альянса [Tabord Ormand](https://zkillboard.com/character/90763727/), который наиболее дотошно относится сливам сокорповцев, утратой флагов... и очень раздражается по утрам, когда видит красную борду. Так вот, для того, чтобы ему крепче спалось, когда всё в порядке, устройство освещает логотип альянса зелёным цветом; красный свет - повод назвать всех раками.

Устройство автономное, получает 5V питание с помощью USB-совместимого провода, получает обновление киллмайлов из Интернет с помощью Wi-Fi подключения, прикидывается светильником (настольной декорацией-безделушкой). И шпионит :0) ... (но это не точно, лолкек).

## Настройка и использование

[![GF Company eveonline](https://img.youtube.com/vi/eDd7JO-nej4/0.jpg)](https://www.youtube.com/watch?v=eDd7JO-nej4 "Tabord RGB (настройка)")

Настройка устройства показана в видеоролике, см. выше.

Процесс настройки сопровождается световой индикацией:

* белый свет - нет подключения к Wi-Fi сети
* зелёный свет - есть подключение к Wi-Fi сети, идёт подключение к Интернету
* синий свет - режим автоматической настройки, выполняется спаривание в WPS режиме
* жёлтый свет - режим ручной настройки, настройки редактируются браузером
* мерцающий красный и мерцающий зелёный свет - отображение результатов боёв альянса

## Самостоятельное изготовление

Устройство сделать несложно самостоятельно, если есть опыт использования паяльника.

В качестве основы использовался Wi-Fi модуль на базе esp8266, а именно LoLin v3 NodeMCU, [например такой](http://roboparts.ru/products/nodemcu-ch340g). Модуль расширения можно изготовить с помощью готового проекта `Tabord RGB`, см. схематику и печатную плату [здесь](https://easyeda.com/search?wd=tabord), которую несложно изготовить с помощью ЛУТ (лазерно-утёжного метода) по вот такой [инструкции](https://habr.com/ru/post/451314/). Подставка и световод изготавливается из подручных материалов; я пользовался фанерой для подставки и 4мм оргстеклом для световода, на котором с помощью бура нацарапал профиль изображения.

<img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200303_093340.jpg" height="30%" width="30%"><img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200305_213225.jpg" height="30%" width="30%"><img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200306_220945.jpg" height="30%" width="30%"><img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200307_192643.jpg" height="30%" width="30%"><img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200309_171512.jpg" height="30%" width="30%">

<img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200307_182509.jpg" height="30%" width="30%"><img src="https://raw.githubusercontent.com/Qandra-Si/tabord_rgb/master/imgs/20200309_173135.jpg" height="30%" width="30%">
