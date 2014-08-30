import liblo
from jack_midi_looper_gui.models import MIDIMappingInfo
from jack_midi_looper_gui.subject import Subject
import subprocess
import threading
import time

class IEngineManager( Subject ):
    """Interface for the engine manager."""
    def __init__( self ):
        """Constructs an engine manager with the given subscription keys."""
        Subject.__init__( self )
        self.add_key( "loops" )
        self.add_key( "mappings" )

    @staticmethod
    def perform_notify( key, callback, data ):
        """
        Implement the Subject's notify functionality.
        
        There is NO guarantee that the provided callbacks will be invoked from the
        same thread, so they should be written accordingly.
        """
        change_type, change_content = data
        callback( change_type, change_content )

    def initialize_subscribers( self ):
        """Retrieve the initial state of the engine."""
        raise NotImplementedError

    def cleanup( self ):
        """Wrap up interaction with the engine."""
        raise NotImplementedError

    def new_loop( self, name ):
        """
        Requests that the engine create a new loop.
        
        Args:
            name (str): A string containing the name of the loop to be created.

        Returns:
            void
        """
        raise NotImplementedError

    def remove_loops( self, names ):
        """
        Requests that the engine remove the given loops.

        Args:
            names (list[str]): A list of string names of loops to be removed.

        Returns:
            void
        """
        raise NotImplementedError

    def new_mapping( self, mapping_info ):
        """
        Requests that the engine create a new MIDI mapping with the given
        characterstics.

        Args:
            mapping_info (MIDIMappingInfo): A MIDIMappingInfo object for the engine to create.

        Returns:
            void
        """
        raise NotImplementedError

    def remove_mappings( self, mapping_infos ):
        """
        Requests that the engine remove all of the specified MIDI mappings.

        Args:
            mapping_infos (list[MIDIMappingInfo]): A list of MIDIMappingInfo objects
            to be removed.

        Returns:
            void
        """
        raise NotImplementedError

def IEngineManagerFactory( engine_port, engine_host, our_port, fail_on_not_found,
    quit_on_shutdown ):
    """Simply construct an appropriate IEngineManager."""
    return EngineManager( engine_port, engine_host, our_port, fail_on_not_found,
        quit_on_shutdown )

class EngineManager( IEngineManager ):
    """Default implementation of engine management using OSC."""
    def __init__( self, engine_port, engine_host, our_port, fail_on_not_found,
        quit_on_shutdown ):
        """
        Initialize by establishing communication with an existing engine, or
        spawning a new one if required.

        Args:
            loop_change_handler ((void)(str,str)): The callback to invoke when
                notified by the engine.
            mapping_change_handler ((void)(str,str)): The callback to invoke.
            engine_port (int): The port on which to communicate with the engine
            engine_host (str): The host on which to look for the engine.
            our_port (int): The port on which our OSC server communicates.
            fail_on_not_found (bool): Determines whether or not we should attempt to
                spawn an engine instance in the case that the given one does not
                respond.
        """
        IEngineManager.__init__( self )

        self._quit_on_shutdown = quit_on_shutdown

        try:
            if our_port is None:
                self._server_thread = liblo.ServerThread()
            else:
                self._server_thread = liblo.ServerThread( our_port )
        except liblo.ServerError:
            print( "Problem setting up OSC!" )
            raise

        self._server_thread.add_method( "/pingack", "ssi", self._pingack_callback )
        self._server_thread.add_method(
            "/loop/update", "ss", self._loop_change_callback )
        self._server_thread.add_method(
            "/mapping/update", "ss", self._mapping_change_callback )
        self._server_thread.start()
        self._received_pingack = False
        self._pingack_lock = threading.Lock()

        self._engine_address = liblo.Address( engine_host, engine_port )

        liblo.send(
            self._engine_address, "/ping", self._server_thread.get_url(), "/pingack" )

        # Wait for the pingack.
        time.sleep( 0.7 )

        self._pingack_lock.acquire()
        received = self._received_pingack
        self._pingack_lock.release()

        if not received:
            if fail_on_not_found:
                # TODO: something a little friendlier
                raise EngineManager.NoEngineError
            subprocess.Popen( ["jack_midi_looper", "-p", str( engine_port )] )
            self._engine_address = liblo.Address( "localhost", engine_port )
            time.sleep( 0.3 ) # Maybe a bit hacky...
            liblo.send( self._engine_address, "/ping", self._server_thread.get_url(),
                "/pingack" )
            time.sleep( 0.7 )

            self._pingack_lock.acquire()
            if not self._received_pingack:
                raise EngineManager.NoEngineError
            self._pingack_lock.release()

    class NoEngineError( Exception ):
        pass

    def initialize_subscribers( self ):
        """
        Requests that the engine send us update information necessary to bring us up
        to its current state.
        """
        liblo.send( self._engine_address, "/loop_list",
            self._server_thread.get_url(), "/loop/update" )
        liblo.send( self._engine_address, "/midi_binding_list",
            self._server_thread.get_url(), "/mapping/update" )
        liblo.send( self._engine_address, "/register_auto_update", "loops",
            self._server_thread.get_url(), "/loop/update" )
        liblo.send( self._engine_address, "/register_auto_update", "mappings",
            self._server_thread.get_url(), "/mapping/update" )

    def cleanup( self ):
        """
        Conclude interaction with the engine by unsubscribing and potentially
        quitting.
        """
        if self._quit_on_shutdown:
            liblo.send( self._engine_address, "/quit" )
        else:
            liblo.send( self._engine_address, "/unregister_auto_update", "loops",
                self._server_thread.get_url(), "/loop/update" )
            liblo.send( self._engine_address, "/unregister_auto_update", "mappings",
                self._server_thread.get_url(), "/mapping/update" )

    def _pingack_callback( self, path, args ):
        host_url, version, loopcount = args
        print( "Received pingack from engine on host {0} running version {1}."
            .format( host_url, version ) )
        print( "The engine currently has {0} loops.".format( loopcount ) )

        self._pingack_lock.acquire()
        self._received_pingack = True
        self._pingack_lock.release()

    def _loop_change_callback( self, path, args ):
        self.notify( "loops", args )

    type_serializations = {
        "Note On":"on",
        "Note Off":"off",
        "CC On":"cc_on",
        "CC Off":"cc_off"
    }
    type_deserializations = {
        "on":"Note On",
        "off":"Note Off",
        "cc_on":"CC On",
        "cc_off":"CC Off"
    }
    action_serializations = {
        "Toggle Playback":"toggle_playback",
        "Toggle Recording":"toggle_recording"
    }
    action_deserializations = {
        "toggle_playback":"Toggle Playback",
        "toggle_recording":"Toggle Recording"
    }

    @staticmethod
    def _serialize_mapping( mapping_info ):
        return "{0} {1} {2} {3} {4}".format( mapping_info.channel,
            EngineManager.type_serializations[mapping_info.midi_type], mapping_info.value,
            EngineManager.action_serializations[mapping_info.loop_action], mapping_info.loop_name )

    @staticmethod
    def _deserialize_mapping( mapping_serialization ):
        data = mapping_serialization.split( " " )
        channel = int( data[0] )
        midi_type = EngineManager.type_deserializations[data[1]]
        value = int( data[2] )
        loop_action = EngineManager.action_deserializations[data[3]]
        loop_name = data[4]
        
        return MIDIMappingInfo( channel, midi_type, value, loop_name, loop_action )

    def _mapping_change_callback( self, path, args ):
        change, serialization = args
        deserialized = ( change, self._deserialize_mapping( serialization ) )
        self.notify( "mappings", deserialized )

    def new_loop( self, name ):
        liblo.send( self._engine_address, "/loop_add", name )

    def remove_loops( self, names ):
        for name in names:
            liblo.send( self._engine_address, "/loop_del", name )

    def new_mapping( self, mapping_info ):
        serialization = self._serialize_mapping( mapping_info )
        liblo.send( self._engine_address, "add_midi_binding", serialization )

    def remove_mappings( self, mapping_infos ):
        for info in mapping_infos:
            serialization = self._serialize_mapping( info )
            liblo.send( self._engine_address, "remove_midi_binding", serialization )
            
        
