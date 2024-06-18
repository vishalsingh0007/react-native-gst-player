import React, { Component, createRef } from 'react';
import { requireNativeComponent, View, UIManager, findNodeHandle, AppState, Platform } from 'react-native';
import PropTypes from 'prop-types';

export const GstState = {
    VOID_PENDING: 0,
    NULL: 1,
    READY: 2,
    PAUSED: 3,
    PLAYING: 4,
};

class GstPlayer extends Component {
    currentGstState = undefined;
    appState = 'active';
    isInitialized = false;

    constructor(props) {
        super(props);
        this.playerViewRef = createRef();
    }

    componentDidMount() {
        this.playerHandle = findNodeHandle(this.playerViewRef.current);
        this.appStateListener = AppState.addEventListener('change', this.appStateChanged);
    }

    componentWillUnmount() {
        this.appStateListener.remove();
    }

    appStateChanged = (nextAppState) => {
        if (this.appState.match(/inactive|background/) && nextAppState === 'active') {
            if (Platform.OS === 'ios') {
                this.recreateView();
            }
            this.play();
        } else {
            this.stop();
        }
        this.appState = nextAppState;
    };

    onPlayerInit = () => {
        this.isInitialized = true;
        if (this.props.onPlayerInit) this.props.onPlayerInit();
    };

    onStateChanged = (_message) => {
        const { old_state, new_state } = _message.nativeEvent;
        this.currentGstState = new_state;

        if (old_state === GstState.PAUSED && new_state === GstState.READY) {
            if (Platform.OS === 'ios') this.recreateView();
        }

        if (this.props.onStateChanged) this.props.onStateChanged(old_state, new_state);
    };

    onUriChanged = (_message) => {
        const { new_uri } = _message.nativeEvent;
        if (this.props.onUriChanged) this.props.onUriChanged(new_uri);
        if (this.props.autoPlay) this.play();
    };

    onEOS = () => {
        if (this.props.onEOS) this.props.onEOS();
    };

    onElementError = (_message) => {
        const { source, message, debug_info } = _message.nativeEvent;
        if (this.props.onElementError) this.props.onElementError(source, message, debug_info);
    };

    setGstState = (state) => {
        UIManager.dispatchViewManagerCommand(
            this.playerHandle,
            UIManager.RCTGstPlayer.Commands.setState,
            [state]
        );
    };

    play = () => {
        this.setGstState(GstState.PLAYING);
    };

    pause = () => {
        this.setGstState(GstState.PAUSED);
    };

    stop = () => {
        this.setGstState(GstState.READY);
    };

    recreateView = () => {
        UIManager.dispatchViewManagerCommand(
            this.playerHandle,
            UIManager.RCTGstPlayer.Commands.recreateView,
            []
        );
    };

    render() {
        return (
            <RCTGstPlayer
                autoPlay={this.props.autoPlay}
                uri={this.props.uri || undefined}
                isDebugging={this.props.isDebugging !== undefined ? this.props.isDebugging : false}
                onPlayerInit={this.onPlayerInit}
                onStateChanged={this.onStateChanged}
                onUriChanged={this.onUriChanged}
                onEOS={this.onEOS}
                onElementError={this.onElementError}
                ref={this.playerViewRef}
                {...this.props}
            />
        );
    }
}

GstPlayer.propTypes = {
    uri: PropTypes.string.isRequired,
    autoPlay: PropTypes.bool,
    isDebugging: PropTypes.bool,
    onPlayerInit: PropTypes.func,
    onStateChanged: PropTypes.func,
    onUriChanged: PropTypes.func,
    onEOS: PropTypes.func,
    onElementError: PropTypes.func,
    setGstState: PropTypes.func,
    play: PropTypes.func,
    pause: PropTypes.func,
    stop: PropTypes.func,
    recreateView: PropTypes.func,
    ...View.propTypes,
};

const RCTGstPlayer = requireNativeComponent('RCTGstPlayer', GstPlayer, {
    nativeOnly: { onChange: true },
});

export default GstPlayer;
