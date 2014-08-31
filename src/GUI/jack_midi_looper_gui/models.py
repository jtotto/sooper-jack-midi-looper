from PyQt5 import QtCore
from PyQt5.QtCore import QAbstractListModel, QAbstractTableModel, QModelIndex

class LoopListModel( QAbstractListModel ):
    """QAbstractListModel implementation."""
    def __init__( self ):
        QAbstractListModel.__init__( self )
        self._data_model = []

    def rowCount( self, parent=QModelIndex() ):
        return len( self._data_model )

    def data( self, index, role=QtCore.Qt.DisplayRole ):
        return self._data_model[index.row()] if index.row() < len( self._data_model ) else None

    def removeRow( self, row, parent=QModelIndex() ):
        if row >= len( self._data_model ):
            return False

        self.beginRemoveRows( parent, row, row )
        del self._data_model[row]
        self.endRemoveRows()
        return True
        
    def insertLoopRow( self, row, loop, parent=QModelIndex() ):
        if row > len( self._data_model ):
            return False

        self.beginInsertRows( parent, row, row )
        self._data_model.insert( row, loop )
        self.endInsertRows()
        return True

    def removeLoop( self, name ):
        """Custom method to remove a loop by name."""
        self.removeRow( self._data_model.index( name ) )
        
class MIDIMappingInfo( object ):
    """POD container for mapping information."""
    MIDI_TYPES = ["Note On", "Note Off", "CC On", "CC Off"]
    ACTION_TYPES = ["Toggle Playback", "Toggle Recording"]

    def __init__( self, channel, midi_type, value, loop_name, loop_action ):
        object.__init__( self )

        if channel >= 0 and channel < 16:
            self.channel = channel
        else:
            raise ValueError( "Invalid channel" )

        if midi_type in self.MIDI_TYPES:
            self.midi_type = midi_type
        else:
            raise ValueError( "Invalid MIDI type" )

        if value >= 0 and value < 128:
            self.value = value
        else:
            raise ValueError( "Invalid MIDI value" )

        if loop_action in self.ACTION_TYPES:
            self.loop_action = loop_action
        else:
            raise ValueError( "Invalid loop action" )

        self.loop_name = loop_name

    def __eq__( self, other ):
        return ( self.channel == other.channel and
            self.midi_type == other.midi_type and
            self.value == other.value and
            self.loop_action == other.loop_action and
            self.loop_name == other.loop_name )

    def __ne__( self, other ):
        return not self.__eq__( other )


class MappingTableModel( QAbstractTableModel ):
    """QAbstractTableModel implementation."""
    COLUMNS = ["Channel", "MIDI Type", "MIDI Value", "Loop", "Action"]

    def __init__( self ):
        QAbstractTableModel.__init__( self )
        self._data_model = []

    def rowCount( self, parent=QModelIndex() ):
        return len( self._data_model )

    def columnCount( self, parent=QModelIndex() ):
        return self.COLUMN_COUNT

    def data( self, index, role=QtCore.Qt.DisplayRole ):
        if ( index.row() < len( self._data_model )
            and index.column() < self.COLUMN_COUNT ):
            
            return MappingTableModel.get_mapping_property(
                self._data_model[index.row()], index.column() )

        return None

    def dataRow( self, row ):
        if row < len( self._data_model ):
            return self._data_model[row]

        return None

    def headerData( self, section, orientation, role=QtCore.Qt.DisplayRole ):
        if role == QtCore.Qt.DisplayRole and orientation == QtCore.Qt.Horizontal:
            return self.COLUMNS[section]
        return QAbstractTableModel.headerData( self, section, orientation, role )

    def removeRow( self, row, parent=QModelIndex() ):
        if row >= len( self._data_model ):
            return False

        self.beginRemoveRows( parent, row, row )
        del self._data_model[row]
        self.endRemoveRows()
        return True
        
    def insertMappingRow( self, row, info, parent=QModelIndex() ):
        if row > len( self._data_model ):
            return False

        self.beginInsertRows( parent, row, row )
        self._data_model.insert( row, info )
        self.endInsertRows()
        return True

    def removeMapping( self, info ):
        """Custom method to remove a mapping by its properties."""
        self.removeRow( self._data_model.index( info ) )
