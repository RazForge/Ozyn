import React, { useState, useEffect } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { StatusBar } from 'expo-status-bar';
import AsyncStorage from '@react-native-async-storage/async-storage';

import ChatScreen from './src/screens/ChatScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import ARWEScreen from './src/screens/ARWEScreen';
import VoiceScreen from './src/screens/VisionScreen';

const Stack = createNativeStackNavigator();

const API_BASE = 'http://localhost:9090/ozayn/backend/api';

export default function App() {
    const [sessionId, setSessionId] = useState(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        loadSession();
    }, []);

    const loadSession = async () => {
        try {
            const session = await AsyncStorage.getItem('ozayn_session');
            if (session) {
                setSessionId(session);
            }
        } catch (e) {
            console.error('Failed to load session:', e);
        }
        setLoading(false);
    };

    const login = async (username, password) => {
        try {
            const res = await fetch(`${API_BASE}/auth/login`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            });
            const data = await res.json();
            if (data.success) {
                setSessionId(data.session_id);
                await AsyncStorage.setItem('ozayn_session', data.session_id);
                return true;
            }
            return false;
        } catch (e) {
            console.error('Login failed:', e);
            return false;
        }
    };

    const register = async (username, password, email) => {
        try {
            const res = await fetch(`${API_BASE}/auth/register`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password, email })
            });
            const data = await res.json();
            return data.success;
        } catch (e) {
            console.error('Register failed:', e);
            return false;
        }
    };

    const logout = async () => {
        setSessionId(null);
        await AsyncStorage.removeItem('ozayn_session');
    };

    if (loading) {
        return null;
    }

    return (
        <NavigationContainer>
            <StatusBar style="light" />
            <Stack.Navigator
                screenOptions={{
                    headerStyle: { backgroundColor: '#0a0a0f' },
                    headerTintColor: '#fff',
                    contentStyle: { backgroundColor: '#0a0a0f' }
                }}
            >
                {!sessionId ? (
                    <Stack.Screen name="Login" options={{ headerShown: false }}>
                        {props => <ChatScreen {...props} onLogin={login} onRegister={register} />}
                    </Stack.Screen>
                ) : (
                    <>
                        <Stack.Screen name="Chat" options={{ title: 'Ozayn' }}>
                            {props => <ChatScreen {...props} sessionId={sessionId} onLogout={logout} />}
                        </Stack.Screen>
                        <Stack.Screen name="ARWE" component={ARWEScreen} options={{ title: 'ARWE Systems' }} />
                        <Stack.Screen name="Voice" component={VoiceScreen} options={{ title: 'Voice & Vision' }} />
                        <Stack.Screen name="Settings" component={SettingsScreen} options={{ title: 'Settings' }} />
                    </>
                )}
            </Stack.Navigator>
        </NavigationContainer>
    );
}
