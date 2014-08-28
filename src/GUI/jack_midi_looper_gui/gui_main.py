import argparse
from jack_midi_looper_gui.engine_manager import IEngineManagerFactory
from PyQt5 import QtCore
from PyQt5.QtWidgets import QApplication, QMainWindow
from jack_midi_looper_gui.Ui_MainWindow import Ui_MainWindow

class _LooperWindow( QMainWindow, Ui_MainWindow ):

    loopUpdated = QtCore.pyqtSignal()
    mappingUpdated = QtCore.pyqtSignal()

    def __init__( self, engine_manager, package, version ):
        super( QMainWindow, self ).__init__()
        self._engine_manager = engine_manager
        self.setupUi( self )
        self.setWindowTitle( "{0} v{1}".format( package, version ) )

        #self.loopListView.setModel( 

        self.loopUpdated.connect(
            self._loop_update_handler, type=QtCore.Qt.QueuedConnection )
        self.mappingUpdated.connect(
            self._mapping_update_handler, type=QtCore.Qt.QueuedConnection )
        self._engine_manager.subscribe( "loops", self.signal_loop_update )
        self._engine_manager.subscribe( "mappings", self.signal_mapping_update )

    def signal_loop_update( self, change, data ):
        self.loopUpdated.emit( change, data )

    def signal_mapping_update( self, change, data ):
        self.mappingUpdated.emit( change, data )

    def _loop_update_handler( self, change, data ):
        pass

    def _mapping_update_handler( self, change, data ):
        pass

    # Slots are named in camelcase for consistency with the rest of Qt.
    def newLoop( self ):
        pass

    def removeLoops( self ):
        pass

    def newMapping( self ):
        pass

    def removeMappings( self ):
        pass

class MainApplicationWrapper( object ):
    def __init__( self, *args, **kwargs ):
        import sys
        for key in kwargs:
            setattr( self, key, kwargs[key] )
        self._app = QApplication( sys.argv )

        parser = argparse.ArgumentParser( description="GUI for jack_midi_looper" )
        parser.add_argument( "-e", "--engine-host",
            help="The host on which the looper engine is running",
            default="localhost" )
        parser.add_argument( "-p", "--engine-port",
            help="The port on which the looper engine should be communicated with",
            type=int,
            required=True )
        parser.add_argument( "-o", "--gui-port", type=int,
            help="The port on which the GUI should communicate" )
        parser.add_argument( "-f", "--fail-on-engine-not-found",
            help="If set, do not spawn a new engine locally if not found.",
            action="store_true" )
        parser.add_argument( "-q", "--no-quit",
            help="If set, do not issue a quit command to the engine on exit.",
            action="store_false" )
            
        parsed_args, unknown = parser.parse_known_args()

        self._engine_manager = IEngineManagerFactory(
            parsed_args.engine_port, parsed_args.engine_host,
            parsed_args.gui_port, parsed_args.fail_on_engine_not_found, parsed_args.no_quit )

        self._ui = _LooperWindow( self._engine_manager, self.package, self.version )
        self._engine_manager.subscribe( "loops", _LooperWindow.signal_loop_update )
        self._engine_manager.subscribe( "mappings", _LooperWindow.signal_mapping_update )
        self._engine_manager.initialize_subscribers()

        self._app.aboutToQuit.connect( self._cleanup )

    def _cleanup( self ):
        self._engine_manager.cleanup()

    def run( self ):
        import sys
        self._ui.show()
        sys.exit( self._app.exec_() )
