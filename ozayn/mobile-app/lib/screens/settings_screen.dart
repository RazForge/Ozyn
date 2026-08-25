import 'package:flutter/material.dart';
import '../api/api_client.dart';

class SettingsScreen extends StatelessWidget {
  final OzaynApiClient api;
  final VoidCallback onLogout;

  const SettingsScreen({super.key, required this.api, required this.onLogout});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: ListTile(
              leading: const Icon(Icons.person),
              title: Text(api.user?['username'] ?? 'User'),
              subtitle: const Text('Logged in'),
            ),
          ),
          const SizedBox(height: 8),
          Card(
            child: ListTile(
              leading: const Icon(Icons.phone_android),
              title: const Text('App Version'),
              subtitle: const Text('1.0.0'),
            ),
          ),
          const SizedBox(height: 8),
          Card(
            child: ListTile(
              leading: const Icon(Icons.storage),
              title: const Text('Backend'),
              subtitle: const Text('PHP + SQLite'),
            ),
          ),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            onPressed: () {
              showDialog(
                context: context,
                builder: (ctx) => AlertDialog(
                  backgroundColor: const Color(0xFF161632),
                  title: const Text('Logout'),
                  content: const Text('Are you sure?'),
                  actions: [
                    TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
                    ElevatedButton(
                      onPressed: () {
                        Navigator.pop(ctx);
                        onLogout();
                      },
                      style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFFFF453A)),
                      child: const Text('Logout'),
                    ),
                  ],
                ),
              );
            },
            icon: const Icon(Icons.logout),
            label: const Text('Logout'),
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFFFF453A)),
          ),
        ],
      ),
    );
  }
}
