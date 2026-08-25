import React, { useState, useEffect } from 'react';
import {
    View, Text, ScrollView, TouchableOpacity, StyleSheet, RefreshControl
} from 'react-native';

const API_BASE = 'http://localhost:9090/ozayn/backend/api';

export default function ARWEScreen({ sessionId }) {
    const [systems, setSystems] = useState({});
    const [loading, setLoading] = useState(true);
    const [refreshing, setRefreshing] = useState(false);

    useEffect(() => {
        loadARWEStatus();
    }, []);

    const loadARWEStatus = async () => {
        try {
            const res = await fetch(`${API_BASE}/arwe/status`, {
                headers: { 'X-Session-ID': sessionId }
            });
            const data = await res.json();
            if (data.systems) {
                setSystems(data.systems);
            }
        } catch (e) {
            console.error('Failed to load ARWE status:', e);
            const fallback = {
                edunex: { status: 'unknown', details: {} },
                govyx: { status: 'unknown', details: {} },
                locify: { status: 'unknown', details: {} },
                terrachain: { status: 'unknown', details: {} },
                bilen: { status: 'unknown', details: {} },
                kidane: { status: 'unknown', details: {} },
                canivox: { status: 'unknown', details: {} }
            };
            setSystems(fallback);
        }
        setLoading(false);
        setRefreshing(false);
    };

    const onRefresh = () => {
        setRefreshing(true);
        loadARWEStatus();
    };

    const getStatusColor = (status) => {
        switch (status) {
            case 'online': return '#22c55e';
            case 'offline': return '#ef4444';
            case 'degraded': return '#f59e0b';
            default: return '#666';
        }
    };

    const getSystemIcon = (name) => {
        const icons = {
            edunex: '\u{1F393}',
            govyx: '\u{1F3DB}',
            locify: '\u{1F4CD}',
            terrachain: '\u{1F331}',
            bilen: '\u{1F50D}',
            kidane: '\u{1F6F8}',
            canivox: '\u{1F916}'
        };
        return icons[name] || '\u{2699}';
    };

    const renderSystemCard = (name, info) => (
        <View key={name} style={styles.card}>
            <View style={styles.cardHeader}>
                <Text style={styles.cardIcon}>{getSystemIcon(name)}</Text>
                <Text style={styles.cardTitle}>{name.charAt(0).toUpperCase() + name.slice(1)}</Text>
                <View style={[styles.statusDot, { backgroundColor: getStatusColor(info.status) }]} />
            </View>
            <Text style={[styles.statusText, { color: getStatusColor(info.status) }]}>
                {info.status || 'unknown'}
            </Text>
            {info.details && (
                <View style={styles.cardDetails}>
                    {Object.entries(info.details).map(([key, value]) => (
                        <Text key={key} style={styles.detailText}>
                            {key.replace(/_/g, ' ')}: {value}
                        </Text>
                    ))}
                </View>
            )}
        </View>
    );

    return (
        <ScrollView
            style={styles.container}
            refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} tintColor="#a855f7" />}
        >
            <Text style={styles.title}>ARWE Systems</Text>
            <Text style={styles.subtitle}>Project Status Overview</Text>

            {loading ? (
                <Text style={styles.loading}>Loading...</Text>
            ) : (
                <View style={styles.grid}>
                    {Object.entries(systems).map(([name, info]) => renderSystemCard(name, info))}
                </View>
            )}

            <TouchableOpacity style={styles.refreshBtn} onPress={onRefresh}>
                <Text style={styles.refreshBtnText}>Refresh</Text>
            </TouchableOpacity>

            <View style={{ height: 20 }} />
        </ScrollView>
    );
}

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: '#0a0a0f', padding: 16 },
    title: { color: '#fff', fontSize: 24, fontWeight: 'bold', marginBottom: 4 },
    subtitle: { color: '#666', fontSize: 14, marginBottom: 20 },
    loading: { color: '#666', textAlign: 'center', marginTop: 40 },
    grid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
    card: {
        width: '48%', backgroundColor: '#1a1a2e', borderRadius: 12,
        padding: 14, marginBottom: 12, borderWidth: 1, borderColor: '#333'
    },
    cardHeader: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
    cardIcon: { fontSize: 20, marginRight: 8 },
    cardTitle: { color: '#fff', fontSize: 15, fontWeight: '600', flex: 1 },
    statusDot: { width: 10, height: 10, borderRadius: 5 },
    statusText: { fontSize: 12, fontWeight: '500', marginBottom: 8 },
    cardDetails: { borderTopWidth: 1, borderTopColor: '#333', paddingTop: 8 },
    detailText: { color: '#999', fontSize: 11, marginBottom: 3, textTransform: 'capitalize' },
    refreshBtn: {
        backgroundColor: '#333', borderRadius: 8, padding: 12,
        alignItems: 'center', marginTop: 12
    },
    refreshBtnText: { color: '#fff', fontSize: 14 }
});
