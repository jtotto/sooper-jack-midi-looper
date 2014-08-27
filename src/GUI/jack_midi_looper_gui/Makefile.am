BUILT_SOURCES = Ui_MainWindow.py

Ui_MainWindow.py: looper.ui
	pyuic5 looper.ui -o Ui_MainWindow.py

jack_midi_looper_gui_PYTHON = \
	gui_main.py \
	__init__.py \
	engine_manager.py \
	models.py \
    subject.py \
	Ui_MainWindow.py

jack_midi_looper_guidir = $(pythondir)/jack_midi_looper_gui

