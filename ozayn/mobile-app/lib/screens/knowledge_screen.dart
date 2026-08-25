import 'package:flutter/material.dart';
import '../api/api_client.dart';

class KnowledgeScreen extends StatefulWidget {
  final OzaynApiClient api;
  const KnowledgeScreen({super.key, required this.api});

  @override
  State<KnowledgeScreen> createState() => _KnowledgeScreenState();
}

class _KnowledgeScreenState extends State<KnowledgeScreen> {
  List<dynamic> _items = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final items = await widget.api.listKnowledge();
    if (mounted) setState(() { _items = items; _loading = false; });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Knowledge Base'),
        actions: [
          IconButton(onPressed: _addEntry, icon: const Icon(Icons.add)),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _items.isEmpty
              ? const Center(child: Text('No knowledge entries yet', style: TextStyle(color: Color(0x80EBEBF5))))
              : ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: _items.length,
                  itemBuilder: (ctx, i) {
                    final k = _items[i];
                    final tags = (k['tags'] as List?)?.join(', ') ?? '';
                    return Card(
                      margin: const EdgeInsets.only(bottom: 8),
                      child: ListTile(
                        title: Text(k['title'] ?? 'Untitled', style: const TextStyle(fontWeight: FontWeight.bold)),
                        subtitle: Text(
                          tags.isNotEmpty ? tags : (k['content'] ?? '').toString().substring(0, (k['content'] ?? '').toString().length.clamp(0, 100)),
                          style: const TextStyle(color: Color(0x80EBEBF5)),
                        ),
                        trailing: const Icon(Icons.chevron_right),
                      ),
                    );
                  },
                ),
    );
  }

  Future<void> _addEntry() async {
    final titleCtrl = TextEditingController();
    final contentCtrl = TextEditingController();
    final result = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF161632),
        title: const Text('Add Knowledge'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(controller: titleCtrl, decoration: const InputDecoration(hintText: 'Title')),
            const SizedBox(height: 12),
            TextField(controller: contentCtrl, maxLines: 4, decoration: const InputDecoration(hintText: 'Content')),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          ElevatedButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Add')),
        ],
      ),
    );
    if (result == true && titleCtrl.text.trim().isNotEmpty) {
      await widget.api.addKnowledge(titleCtrl.text.trim(), contentCtrl.text.trim());
      _load();
    }
  }
}
