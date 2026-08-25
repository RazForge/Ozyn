import React, { useState, useEffect } from 'react';
import {
    View, Text, TextInput, TouchableOpacity, ScrollView,
    StyleSheet, Switch, Alert
} from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';

export default function SettingsScreen({ onLogout }) {
    const [settings, setSettings] = useState({
        aiProvider: 'demo',
        aiApiKey: '',
        aiModel: '',
        voiceLanguage: 'en-US',
        autoSpeak: false,
        theme: 'dark',
        serverUrl: 'http://localhost:9090'
    });

    useEffect(() => {
        loadSettings();
    }, []);

    const loadSettings = async () => {
        try {
            const saved = await AsyncStorage.getItem('ozayn_settings');
            if (saved) {
                setSettings({ ...settings, ...JSON.parse(saved) });
            }
        } catch (e) {
            console.error('Failed to load settings:', e);
        }
    };

    const saveSettings = async () => {
        try {
            await AsyncStorage.setItem('ozayn_settings', JSON.stringify(settings));
            Alert.alert('Success', 'Settings saved');
        } catch (e) {
            Alert.alert('Error', 'Failed to save settings');
        }
    };

    const updateSetting = (key, value) => {
        setSettings({ ...settings, [key]: value });
    };

    return (
        <ScrollView style={styles.container}>
            <View style={styles.section}>
                <Text style={styles.sectionTitle}>AI Configuration</Text>

                <Text style={styles.label}>AI Provider</Text>
                <View style={styles.radioGroup}>
                    {['demo', 'openai', 'anthropic', 'local'].map(provider => (
                        <TouchableOpacity
                            key={provider}
                            style={[styles.radio, settings.aiProvider === provider && styles.radioActive]}
                            onPress={() => updateSetting('aiProvider', provider)}
                        >
                            <Text style={[styles.radioText, settings.aiProvider === provider && styles.radioTextActive]}>
                                {provider.charAt(0).toUpperCase() + provider.slice(1)}
                            </Text>
                        </TouchableOpacity>
                    ))}
                </View>

                <Text style={styles.label}>API Key</Text>
                <TextInput
                    style={styles.input}
                    value={settings.aiApiKey}
                    onChangeText={(v) => updateSetting('aiApiKey', v)}
                    placeholder="Enter API key"
                    placeholderTextColor="#666"
                    secureTextEntry
                />

                <Text style={styles.label}>Model</Text>
                <TextInput
                    style={styles.input}
                    value={settings.aiModel}
                    onChangeText={(v) => updateSetting('aiModel', v)}
                    placeholder="e.g., gpt-4"
                    placeholderTextColor="#666"
                />
            </View>

            <View style={styles.section}>
                <Text style={styles.sectionTitle}>Voice</Text>

                <Text style={styles.label}>Language</Text>
                <View style={styles.radioGroup}>
                    {[
                        { code: 'en-US', name: 'English' },
                        { code: 'am-ET', name: 'Amharic' },
                        { code: 'om-ET', name: 'Afaan Oromo' }
                    ].map(lang => (
                        <TouchableOpacity
                            key={lang.code}
                            style={[styles.radio, settings.voiceLanguage === lang.code && styles.radioActive]}
                            onPress={() => updateSetting('voiceLanguage', lang.code)}
                        >
                            <Text style={[styles.radioText, settings.voiceLanguage === lang.code && styles.radioTextActive]}>
                                {lang.name}
                            </Text>
                        </TouchableOpacity>
                    ))}
                </View>

                <View style={styles.switchRow}>
                    <Text style={styles.label}>Auto-speak responses</Text>
                    <Switch
                        value={settings.autoSpeak}
                        onValueChange={(v) => updateSetting('autoSpeak', v)}
                        trackColor={{ false: '#333', true: '#a855f7' }}
                    />
                </View>
            </View>

            <View style={styles.section}>
                <Text style={styles.sectionTitle}>Appearance</Text>

                <Text style={styles.label}>Theme</Text>
                <View style={styles.radioGroup}>
                    {['dark', 'midnight', 'purple', 'green'].map(theme => (
                        <TouchableOpacity
                            key={theme}
                            style={[styles.radio, settings.theme === theme && styles.radioActive]}
                            onPress={() => updateSetting('theme', theme)}
                        >
                            <Text style={[styles.radioText, settings.theme === theme && styles.radioTextActive]}>
                                {theme.charAt(0).toUpperCase() + theme.slice(1)}
                            </Text>
                        </TouchableOpacity>
                    ))}
                </View>
            </View>

            <View style={styles.section}>
                <Text style={styles.sectionTitle}>Server</Text>

                <Text style={styles.label}>Server URL</Text>
                <TextInput
                    style={styles.input}
                    value={settings.serverUrl}
                    onChangeText={(v) => updateSetting('serverUrl', v)}
                    placeholder="http://localhost:9090"
                    placeholderTextColor="#666"
                    autoCapitalize="none"
                />
            </View>

            <TouchableOpacity style={styles.saveBtn} onPress={saveSettings}>
                <Text style={styles.saveBtnText}>Save Settings</Text>
            </TouchableOpacity>

            <TouchableOpacity style={styles.logoutBtn} onPress={onLogout}>
                <Text style={styles.logoutBtnText}>Logout</Text>
            </TouchableOpacity>

            <View style={{ height: 40 }} />
        </ScrollView>
    );
}

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: '#0a0a0f', padding: 16 },
    section: {
        backgroundColor: '#1a1a2e', borderRadius: 12, padding: 16,
        marginBottom: 16, borderWidth: 1, borderColor: '#333'
    },
    sectionTitle: { color: '#a855f7', fontSize: 16, fontWeight: '600', marginBottom: 16 },
    label: { color: '#fff', fontSize: 14, marginBottom: 8 },
    input: {
        backgroundColor: '#0a0a0f', borderRadius: 8, padding: 12,
        color: '#fff', fontSize: 14, borderWidth: 1, borderColor: '#333', marginBottom: 12
    },
    radioGroup: { flexDirection: 'row', flexWrap: 'wrap', marginBottom: 12 },
    radio: {
        backgroundColor: '#0a0a0f', borderRadius: 8, paddingHorizontal: 16,
        paddingVertical: 8, marginRight: 8, marginBottom: 8, borderWidth: 1, borderColor: '#333'
    },
    radioActive: { backgroundColor: '#a855f7', borderColor: '#a855f7' },
    radioText: { color: '#999', fontSize: 13 },
    radioTextActive: { color: '#fff' },
    switchRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
    saveBtn: {
        backgroundColor: '#a855f7', borderRadius: 8, padding: 14,
        alignItems: 'center', marginBottom: 12
    },
    saveBtnText: { color: '#fff', fontSize: 16, fontWeight: '600' },
    logoutBtn: {
        backgroundColor: 'transparent', borderRadius: 8, padding: 14,
        alignItems: 'center', borderWidth: 1, borderColor: '#ef4444'
    },
    logoutBtnText: { color: '#ef4444', fontSize: 16 }
});
