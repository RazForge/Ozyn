import 'package:flutter/material.dart';
import '../api/api_client.dart';

class DecisionsScreen extends StatefulWidget {
  final OzaynApiClient api;
  const DecisionsScreen({super.key, required this.api});

  @override
  State<DecisionsScreen> createState() => _DecisionsScreenState();
}

class _DecisionsScreenState extends State<DecisionsScreen> {
  List<dynamic> _decisions = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final decisions = await widget.api.listDecisions();
    if (mounted) setState(() { _decisions = decisions; _loading = false; });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Decisions'),
        actions: [
          IconButton(onPressed: _newDecision, icon: const Icon(Icons.add)),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _decisions.isEmpty
              ? const Center(child: Text('No decisions yet', style: TextStyle(color: Color(0x80EBEBF5))))
              : ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: _decisions.length,
                  itemBuilder: (ctx, i) {
                    final d = _decisions[i];
                    final status = d['status'] ?? '';
                    final chosen = d['chosen_option'] ?? '';
                    return Card(
                      margin: const EdgeInsets.only(bottom: 8),
                      child: ListTile(
                        title: Text(d['context'] ?? '', maxLines: 2, overflow: TextOverflow.ellipsis),
                        subtitle: Text(
                          '[$status]' + (chosen.isNotEmpty ? ' — $chosen' : ''),
                          style: const TextStyle(color: Color(0x80EBEBF5)),
                        ),
                      ),
                    );
                  },
                ),
    );
  }

  Future<void> _newDecision() async {
    final ctrl = TextEditingController();
    final result = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF161632),
        title: const Text('New Decision'),
        content: TextField(controller: ctrl, maxLines: 3, decoration: const InputDecoration(hintText: 'Describe the decision context')),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          ElevatedButton(onPressed: () => Navigator.pop(ctx, ctrl.text), child: const Text('Create')),
        ],
      ),
    );
    if (result != null && result.trim().isNotEmpty) {
      await widget.api.createDecision(result.trim());
      _load();
    }
  }
}
