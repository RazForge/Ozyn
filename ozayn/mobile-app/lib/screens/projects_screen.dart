import 'package:flutter/material.dart';
import '../api/api_client.dart';

class ProjectsScreen extends StatefulWidget {
  final OzaynApiClient api;
  const ProjectsScreen({super.key, required this.api});

  @override
  State<ProjectsScreen> createState() => _ProjectsScreenState();
}

class _ProjectsScreenState extends State<ProjectsScreen> {
  List<dynamic> _projects = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final projects = await widget.api.listProjects();
    if (mounted) setState(() { _projects = projects; _loading = false; });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Projects'),
        actions: [
          IconButton(onPressed: _newProject, icon: const Icon(Icons.add)),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _projects.isEmpty
              ? const Center(child: Text('No projects yet', style: TextStyle(color: Color(0x80EBEBF5))))
              : ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: _projects.length,
                  itemBuilder: (ctx, i) {
                    final p = _projects[i];
                    return Card(
                      margin: const EdgeInsets.only(bottom: 8),
                      child: ListTile(
                        title: Text(p['name'] ?? 'Untitled', style: const TextStyle(fontWeight: FontWeight.bold)),
                        subtitle: Text(p['description'] ?? 'No description', style: const TextStyle(color: Color(0x80EBEBF5))),
                        trailing: const Icon(Icons.chevron_right),
                      ),
                    );
                  },
                ),
    );
  }

  Future<void> _newProject() async {
    final ctrl = TextEditingController();
    final result = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF161632),
        title: const Text('New Project'),
        content: TextField(controller: ctrl, decoration: const InputDecoration(hintText: 'Project name')),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          ElevatedButton(onPressed: () => Navigator.pop(ctx, ctrl.text), child: const Text('Create')),
        ],
      ),
    );
    if (result != null && result.trim().isNotEmpty) {
      await widget.api.createProject(result.trim());
      _load();
    }
  }
}
