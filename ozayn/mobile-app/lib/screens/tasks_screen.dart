import 'package:flutter/material.dart';
import '../api/api_client.dart';

class TasksScreen extends StatefulWidget {
  final OzaynApiClient api;
  const TasksScreen({super.key, required this.api});

  @override
  State<TasksScreen> createState() => _TasksScreenState();
}

class _TasksScreenState extends State<TasksScreen> {
  List<dynamic> _tasks = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final tasks = await widget.api.listTasks();
    if (mounted) setState(() { _tasks = tasks; _loading = false; });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Tasks'),
        actions: [
          IconButton(onPressed: _newTask, icon: const Icon(Icons.add)),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _tasks.isEmpty
              ? const Center(child: Text('No tasks yet', style: TextStyle(color: Color(0x80EBEBF5))))
              : ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: _tasks.length,
                  itemBuilder: (ctx, i) {
                    final t = _tasks[i];
                    final done = t['status'] == 'completed';
                    final priority = t['priority'] ?? 'medium';
                    final color = priority == 'high'
                        ? const Color(0xFFFF453A)
                        : priority == 'medium'
                            ? const Color(0xFFFF9F0A)
                            : const Color(0xFF30D158);
                    return Card(
                      margin: const EdgeInsets.only(bottom: 8),
                      child: ListTile(
                        leading: Icon(
                          done ? Icons.check_circle : Icons.circle_outlined,
                          color: done ? const Color(0xFF30D158) : const Color(0x80EBEBF5),
                        ),
                        title: Text(
                          t['title'] ?? 'Untitled',
                          style: TextStyle(
                            fontWeight: FontWeight.bold,
                            decoration: done ? TextDecoration.lineThrough : null,
                            color: done ? const Color(0x40EBEBF5) : const Color(0xFFEBEBF5),
                          ),
                        ),
                        subtitle: Row(
                          children: [
                            Container(
                              padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                              decoration: BoxDecoration(
                                color: color.withOpacity(0.2),
                                borderRadius: BorderRadius.circular(4),
                              ),
                              child: Text(priority, style: TextStyle(fontSize: 11, color: color)),
                            ),
                          ],
                        ),
                        onTap: () {
                          widget.api.updateTask(t['id'], done ? 'pending' : 'completed');
                          _load();
                        },
                      ),
                    );
                  },
                ),
    );
  }

  Future<void> _newTask() async {
    final ctrl = TextEditingController();
    String priority = 'medium';
    final result = await showDialog<Map<String, dynamic>>(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setDialogState) => AlertDialog(
          backgroundColor: const Color(0xFF161632),
          title: const Text('New Task'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(controller: ctrl, decoration: const InputDecoration(hintText: 'Task title')),
              const SizedBox(height: 12),
              DropdownButtonFormField<String>(
                value: priority,
                decoration: const InputDecoration(labelText: 'Priority'),
                items: const [
                  DropdownMenuItem(value: 'low', child: Text('Low')),
                  DropdownMenuItem(value: 'medium', child: Text('Medium')),
                  DropdownMenuItem(value: 'high', child: Text('High')),
                ],
                onChanged: (v) => setDialogState(() => priority = v ?? 'medium'),
              ),
            ],
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
            ElevatedButton(
              onPressed: () => Navigator.pop(ctx, {'title': ctrl.text, 'priority': priority}),
              child: const Text('Create'),
            ),
          ],
        ),
      ),
    );
    if (result != null && result['title']?.trim().isNotEmpty == true) {
      await widget.api.createTask(result['title']!.trim(), priority: result['priority']);
      _load();
    }
  }
}
