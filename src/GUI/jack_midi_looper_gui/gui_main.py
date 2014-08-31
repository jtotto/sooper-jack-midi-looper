import argparse
import logging
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtWidgets import QApplication, QMainWindow, QDialog
from jack_midi_looper_gui.engine_manager import IEngineManagerFactory
from jack_midi_looper_gui.Ui_MainWindow import Ui_MainWindow
from jack_midi_looper_gui.Ui_MappingDialog import Ui_MappingDialog
from jack_midi_looper_gui.models import LoopListModel, MappingTableModel, MIDIMappingInfo

class _LooperWindow( QMainWindow, Ui_MainWindow ):

    loopUpdated = QtCore.pyqtSignal( str, str )
    mappingUpdated = QtCore.pyqtSignal( str, MIDIMappingInfo )

    def __init__( self, engine_manager, package, version ):
        super( QMainWindow, self ).__init__()
        self._engine_manager = engine_manager
        self.setupUi( self )
        self.setWindowTitle( "{0} v{1}".format( package, version ) )

        self.loopListViewModel = LoopListModel()
        self.loopListView.setModel( self.loopListViewModel )

        self.mappingTableViewModel = MappingTableModel()
        self.mappingTableView.setModel( self.mappingTableViewModel )
        tableHeader = self.mappingTableView.horizontalHeader()
        tableHeader.setSectionResizeMode( QtWidgets.QHeaderView.Stretch )

        # Only update the model on the GUI thread - thread-safe.
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
        if change == "add":
            self.loopListViewModel.insertLoopRow(
                self.loopListViewModel.rowCount(), data )
        elif change == "remove":
            self.loopListViewModel.removeLoop( data )
        else:
            raise ValueError( "Invalid loop update!" )

    def _mapping_update_handler( self, change, data ):
        if change == "add":
            self.mappingTableViewModel.insertMappingRow(
                self.mappingTableViewModel.rowCount(), data )
        elif change == "remove":
            self.mappingTableViewModel.removeMapping( data )
        else:
            raise ValueError( "Invalid mapping update!" )

    # Slots are named in camelcase for consistency with the rest of Qt.
    def newLoop( self ):
        new_loop_name, ok = QtWidgets.QInputDialog.getText( self, "New Loop",
            "Provide the name for the new loop." )
        if ok:
            self._engine_manager.new_loop( new_loop_name )

    def removeLoops( self ):
        # Gets the names because a normal iterative removal would invalidate the
        # index list at each step.  This is not particularly efficient.
        selected = [self.loopListViewModel.data( x ) for x in
            self.loopListView.selectedIndexes()]
        self._engine_manager.remove_loops( selected )

    def newMapping( self ):
        loops = [
            self.loopListViewModel.data( self.loopListViewModel.createIndex( x, 0 ) )
                for x in range( 0, self.loopListViewModel.rowCount() ) ]

        if not loops:
            QtWidgets.QMessageBox.warning( self, "No loops",
                "You must add a loop before defining mappings." )
            return

        mapping_dialog = _MappingDialog( loops, self )
        if mapping_dialog.exec_():
            channel, midi_type, value, name, action = mapping_dialog.getMappingData()
            self._engine_manager.new_mapping(
                MIDIMappingInfo( channel, midi_type, value, name, action ) )

    def removeMappings( self ):
        mappings = [self.mappingTableViewModel.dataRow( x )
            for x in self.mappingTableView.selectionModel().selectedRows()]
        self._engine_manager.remove_mappings( mappings )

class _MappingDialog( QDialog, Ui_MappingDialog ):
    def __init__( self, loops, parent=None ):
        super( QDialog, self ).__init__( parent )
        self.setupUi( self )

        self.comboBox_loop.clear()
        self.comboBox_loop.addItems( loops )

    def getMappingData( self ):
        return ( self.spinBox_channel.value() - 1, str( self.comboBox_type.currentText() ),
            self.spinBox_midi_value.value(),  str( self.comboBox_loop.currentText() ),
            str( self.comboBox_action.currentText() ) )

class MainApplicationWrapper( object ):
    def __init__( self, *args, **kwargs ):
        import sys
        for key in kwargs:
            setattr( self, key, kwargs[key] )
        self._app = QApplication( sys.argv )

        logging.basicConfig( level=self.log_level )
        logging.info( "Built with logging level info." )

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
        self._engine_manager.initialize_subscribers()

        self._app.aboutToQuit.connect( self._cleanup )

    def _cleanup( self ):
        self._engine_manager.cleanup()

    def run( self ):
        import sys
        self._ui.show()
        sys.exit( self._app.exec_() )
