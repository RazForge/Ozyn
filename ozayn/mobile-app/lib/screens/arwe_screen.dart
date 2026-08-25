import 'package:flutter/material.dart';
import '../api/api_client.dart';

class ARWEScreen extends StatefulWidget {
  final OzaynApiClient api;
  const ARWEScreen({super.key, required this.api});

  @override
  State<ARWEScreen> createState() => _ARWEScreenState();
}

class _ARWEScreenState extends State<ARWEScreen> {
  Map<String, dynamic> _systems = {};
  String _briefing = '';
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final status = await widget.api.arweStatus();
    final briefing = await widget.api.arweBriefing();
    if (mounted) {
      setState(() {
        _systems = status;
        _briefing = briefing['briefing'] ?? '';
        _loading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ARWE Systems'),
        actions: [
          IconButton(onPressed: _load, icon: const Icon(Icons.refresh)),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : RefreshIndicator(
              onRefresh: _load,
              child: ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  ..._systems.entries.map((e) {
                    final info = e.value;
                    if (info is! Map) return const SizedBox();
                    final status = info['status'] ?? 'unknown';
                    final isOnline = status == 'online';
                    final details = info['details'] as Map? ?? {};
                    return Card(
                      margin: const EdgeInsets.only(bottom: 12),
                      child: Padding(
                        padding: const EdgeInsets.all(16),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Row(
                              children: [
                                Text(e.key.toUpperCase(), style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                                const Spacer(),
                                Container(
                                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                                  decoration: BoxDecoration(
                                    color: (isOnline ? const Color(0xFF30D158) : const Color(0xFFFF453A)).withOpacity(0.2),
                                    borderRadius: BorderRadius.circular(8),
                                  ),
                                  child: Text(status, style: TextStyle(
                                    fontSize: 12,
                                    fontWeight: FontWeight.bold,
                                    color: isOnline ? const Color(0xFF30D158) : const Color(0xFFFF453A),
                                  )),
                                ),
                              ],
                            ),
                            if (details.isNotEmpty) ...[
                              const SizedBox(height: 8),
                              Text(
                                details.entries.map((d) => '${d.key}: ${d.value}').join(' | '),
                                style: const TextStyle(fontSize: 12, color: Color(0x80EBEBF5)),
                              ),
                            ],
                          ],
                        ),
                      ),
                    );
                  }),
                  if (_briefing.isNotEmpty) ...[
                    const SizedBox(height: 16),
                    const Text('Daily Briefing', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Color(0xFF0A84FF))),
                    const SizedBox(height: 8),
                    Text(_briefing, style: const TextStyle(color: Color(0xB3EBEBF5))),
                  ],
                ],
              ),
            ),
    );
  }
}
