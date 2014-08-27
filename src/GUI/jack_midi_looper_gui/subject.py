class Subject( object ):
    """The abstract Subject of the observer pattern."""
    def __init__( self ):
        self._observers = {}

    def add_key( self, key ):
        """
        Adds a new key to the pool of keys available for subscription.

        Args:
            key (str): a string on which subscriptions should now be allowed.

        Returns:
            void
        """
        if not key in self._observers:
            self._observers[key] = set()

    def remove_key( self, key ):
        """
        Removes a key that was previously available for subscription from the pool.

        Args:
            key (str): the key to be removed

        Returns:
            void
        """
        self._observers.pop( key )

    def subscribe( self, key, callback ):
        """
        Subscribe the callback to the subject on the given key.

        Args:
            key (str): the key on which the subscription is to be made
            callback (callable): the callable to be invoked on notifications

        Returns:
            void
        """
        self._observers[key].add( callback )

    def unsubscribe( self, key, callback ):
        """
        Unsubscribes the callback from the subject on the given key.

        Args:
            key (str): the key to which the subscription was made
            callback (callable): a reference to the callback to be unsubscribed.

        Returns:
            void
        """
        self._observers[key].remove( callback )

    def notify( self, key, data ):
        """
        Notify all subscribers on the given key by invoking their callbacks.

        Args:
            key (str): the key on which to notify
            data (tuple): an arbitrary tuple containing argument data

        Returns:
            void
        """
        for callback in self._observers[key]:
            perform_notify( key, callback, data )

    @staticmethod
    def perform_notify( key, callback, data ):
        """
        Performs a notification.  Must be overridden by each subject.

        Args:
            key (str): the key on which the notification is occurring.
            callback (callable): the observer callback
            data (tuple): an arbitrary tuple containing argument data

        Returns:
            void
        """
        raise NotImplementedError


