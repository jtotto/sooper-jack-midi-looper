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

    def setData( self, index, value, role=QtCore.Qt.EditRole ):
        if index.row() >= len( self._data_model ):
            return False

        self._data_model[index.row()] = value
        self.dataChanged.emit( index, index, () )
        return True

    def flags( self, index ):
        return QAbstractListModel.flags( self, index ) | QtCore.Qt.ItemIsEditable

    def removeRow( self, row, parent=QModelIndex() ):
        if row >= len( self._data_model ):
            return False

        self.beginRemoveRows( parent, row, row )
        del self._data_model[row]
        self.endRemoveRows()
        return True
        
    def insertRow( self, row, parent=QModelIndex() ):
        if row > len( self._data_model ):
            return False

        self.beginInsertRows( parent, row, row )
        self._data_model.insert( row, None )
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
    COLUMN_CHANNEL = 0
    COLUMN_TYPE = 1
    COLUMN_VALUE = 2
    COLUMN_LOOP_NAME = 3
    COLUMN_ACTION = 4
    COLUMN_COUNT = 5
    COLUMNS = ["Channel", "MIDI Type", "MIDI Value", "Loop", "Action"]

    # Is there an idiomatic Python way of doing this?
    @staticmethod
    def get_mapping_property( info, column ):
        if column == MappingTableModel.COLUMN_CHANNEL:
            return info.channel
        elif column == MappingTableModel.COLUMN_TYPE:
            return info.midi_type
        elif column == MappingTableModel.COLUMN_VALUE:
            return info.value
        elif column == MappingTableModel.COLUMN_LOOP_NAME:
            return info.loop_name
        elif column == MappingTableModel.COLUMN_ACTION:
            return info.loop_action
        else:
            raise ValueError( "Invalid column." )

    @staticmethod
    def set_mapping_property( info, column, data ):
        if column == MappingTableModel.COLUMN_CHANNEL:
            info.channel = data
        elif column == MappingTableModel.COLUMN_TYPE:
            info.midi_type = data
        elif column == MappingTableModel.COLUMN_VALUE:
            info.value = data
        elif column == MappingTableModel.COLUMN_LOOP_NAME:
            info.loop_name = data
        elif column == MappingTableModel.COLUMN_ACTION:
            info.loop_action = data
        else:
            raise ValueError( "Invalid column." )

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

    def setData( self, index, value, role=QtCore.Qt.EditRole ):
        if ( index.row() >= len( self._data_model ) or
            index.column() >= self.COLUMN_COUNT ):

            return False

        print( index.row(), len( self._data_model ) )
        MappingTableModel.set_mapping_property(
            self._data_model[index.row()], index.column(), value )
        self.dataChanged.emit( index, index, () )
        return True

    def flags( self, index ):
        return QAbstractTableModel.flags( self, index ) | QtCore.Qt.ItemIsEditable

    def removeRow( self, row, parent=QModelIndex() ):
        if row >= len( self._data_model ):
            return False

        self.beginRemoveRows( parent, row, row )
        del self._data_model[row]
        self.endRemoveRows()
        return True
        
    def insertRow( self, row, parent=QModelIndex() ):
        if row > len( self._data_model ):
            return False

        self.beginInsertRows( parent, row, row )
        self._data_model.insert( row, None )
        self.endInsertRows()
        return True

    def removeMapping( self, info ):
        """Custom method to remove a mapping by its properties."""
        self.removeRow( self._data_model.index( info ) )
