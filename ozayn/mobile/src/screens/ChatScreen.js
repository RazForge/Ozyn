import React, { useState, useEffect, useRef } from 'react';
import {
    View, Text, TextInput, TouchableOpacity, FlatList,
    StyleSheet, KeyboardAvoidingView, Platform, ActivityIndicator
} from 'react-native';

const API_BASE = 'http://localhost:9090/ozayn/backend/api';

export default function ChatScreen({ sessionId, onLogout, onLogin, onRegister }) {
    const [messages, setMessages] = useState([]);
    const [input, setInput] = useState('');
    const [loading, setLoading] = useState(false);
    const [isAuthenticated, setIsAuthenticated] = useState(!!sessionId);
    const [authMode, setAuthMode] = useState('login');
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const flatListRef = useRef();

    useEffect(() => {
        if (sessionId) {
            setIsAuthenticated(true);
            loadHistory();
        }
    }, [sessionId]);

    const loadHistory = async () => {
        try {
            const res = await fetch(`${API_BASE}/chat/list`, {
                headers: { 'X-Session-ID': sessionId }
            });
            const data = await res.json();
            if (data.conversations) {
                setMessages([{ type: 'system', text: 'Welcome to Ozayn. Type a message to start.' }]);
            }
        } catch (e) {
            console.error('Failed to load history:', e);
        }
    };

    const sendMessage = async () => {
        if (!input.trim()) return;

        const userMsg = { type: 'user', text: input.trim() };
        setMessages(prev => [...prev, userMsg]);
        setInput('');
        setLoading(true);

        try {
            const res = await fetch(`${API_BASE}/chat/send`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'X-Session-ID': sessionId
                },
                body: JSON.stringify({ message: input.trim() })
            });
            const data = await res.json();
            if (data.response) {
                setMessages(prev => [...prev, { type: 'assistant', text: data.response }]);
            }
        } catch (e) {
            setMessages(prev => [...prev, { type: 'error', text: 'Failed to send message' }]);
        }
        setLoading(false);
    };

    const handleAuth = async () => {
        setLoading(true);
        let success;
        if (authMode === 'login') {
            success = await onLogin(username, password);
        } else {
            success = await onRegister(username, password);
        }
        setLoading(false);
        if (!success) {
            alert('Authentication failed');
        }
    };

    if (!isAuthenticated) {
        return (
            <View style={styles.authContainer}>
                <Text style={styles.logo}>Ozayn</Text>
                <Text style={styles.subtitle}>AI Digital Twin</Text>
                
                <TextInput
                    style={styles.input}
                    placeholder="Username"
                    placeholderTextColor="#666"
                    value={username}
                    onChangeText={setUsername}
                />
                <TextInput
                    style={styles.input}
                    placeholder="Password"
                    placeholderTextColor="#666"
                    value={password}
                    onChangeText={setPassword}
                    secureTextEntry
                />
                
                <TouchableOpacity style={styles.authButton} onPress={handleAuth} disabled={loading}>
                    {loading ? (
                        <ActivityIndicator color="#fff" />
                    ) : (
                        <Text style={styles.authButtonText}>{authMode === 'login' ? 'Login' : 'Register'}</Text>
                    )}
                </TouchableOpacity>
                
                <TouchableOpacity onPress={() => setAuthMode(authMode === 'login' ? 'register' : 'login')}>
                    <Text style={styles.authToggle}>
                        {authMode === 'login' ? 'Create account' : 'Login instead'}
                    </Text>
                </TouchableOpacity>
            </View>
        );
    }

    const renderMessage = ({ item }) => (
        <View style={[styles.message, styles[`message_${item.type}`]]}>
            <Text style={styles.messageText}>{item.text}</Text>
        </View>
    );

    return (
        <KeyboardAvoidingView 
            style={styles.container}
            behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
        >
            <FlatList
                ref={flatListRef}
                data={messages}
                renderItem={renderMessage}
                keyExtractor={(item, index) => index.toString()}
                style={styles.messageList}
                onContentSizeChange={() => flatListRef.current?.scrollToEnd()}
            />
            
            {loading && (
                <View style={styles.loadingContainer}>
                    <ActivityIndicator color="#a855f7" />
                </View>
            )}
            
            <View style={styles.inputContainer}>
                <TextInput
                    style={styles.chatInput}
                    placeholder="Message Ozayn..."
                    placeholderTextColor="#666"
                    value={input}
                    onChangeText={setInput}
                    onSubmitEditing={sendMessage}
                    multiline
                />
                <TouchableOpacity style={styles.sendButton} onPress={sendMessage}>
                    <Text style={styles.sendButtonText}>Send</Text>
                </TouchableOpacity>
            </View>
        </KeyboardAvoidingView>
    );
}

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: '#0a0a0f' },
    authContainer: { flex: 1, backgroundColor: '#0a0a0f', justifyContent: 'center', alignItems: 'center', padding: 20 },
    logo: { fontSize: 48, fontWeight: 'bold', color: '#a855f7', marginBottom: 8 },
    subtitle: { fontSize: 16, color: '#666', marginBottom: 40 },
    input: { width: '100%', maxWidth: 300, backgroundColor: '#1a1a2e', borderRadius: 8, padding: 14, color: '#fff', marginBottom: 12, borderWidth: 1, borderColor: '#333' },
    authButton: { width: '100%', maxWidth: 300, backgroundColor: '#a855f7', borderRadius: 8, padding: 14, alignItems: 'center', marginBottom: 16 },
    authButtonText: { color: '#fff', fontSize: 16, fontWeight: '600' },
    authToggle: { color: '#a855f7', fontSize: 14 },
    messageList: { flex: 1, padding: 16 },
    message: { maxWidth: '80%', padding: 12, borderRadius: 12, marginBottom: 8 },
    message_user: { backgroundColor: '#a855f7', alignSelf: 'flex-end', borderBottomRightRadius: 4 },
    message_assistant: { backgroundColor: '#1a1a2e', alignSelf: 'flex-start', borderBottomLeftRadius: 4 },
    message_system: { backgroundColor: 'transparent', alignSelf: 'center', opacity: 0.6 },
    message_error: { backgroundColor: '#ef4444', alignSelf: 'center' },
    messageText: { color: '#fff', fontSize: 15, lineHeight: 20 },
    loadingContainer: { padding: 8, alignItems: 'center' },
    inputContainer: { flexDirection: 'row', padding: 12, backgroundColor: '#1a1a2e', borderTopWidth: 1, borderTopColor: '#333' },
    chatInput: { flex: 1, color: '#fff', fontSize: 16, maxHeight: 100, marginRight: 12 },
    sendButton: { backgroundColor: '#a855f7', borderRadius: 8, paddingHorizontal: 20, justifyContent: 'center' },
    sendButtonText: { color: '#fff', fontWeight: '600' }
});
