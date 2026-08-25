import 'package:flutter/material.dart';
import '../api/api_client.dart';

class ChatScreen extends StatefulWidget {
  final OzaynApiClient api;
  const ChatScreen({super.key, required this.api});

  @override
  State<ChatScreen> createState() => _ChatScreenState();
}

class _ChatScreenState extends State<ChatScreen> {
  final _inputCtrl = TextEditingController();
  final _scrollCtrl = ScrollController();
  final List<Map<String, dynamic>> _messages = [];
  String? _convId;
  bool _loading = false;

  @override
  void initState() {
    super.initState();
    _loadConversations();
  }

  Future<void> _loadConversations() async {
    final convs = await widget.api.listConversations();
    if (convs.isNotEmpty && mounted) {
      setState(() {
        _convId = convs.first['id']?.toString();
      });
      if (_convId != null) _loadHistory(_convId!);
    }
  }

  Future<void> _loadHistory(String convId) async {
    final msgs = await widget.api.getChatHistory(convId);
    if (mounted) {
      setState(() {
        _messages.clear();
        for (final m in msgs) {
          _messages.add({
            'role': m['role'] ?? 'user',
            'content': m['content'] ?? '',
          });
        }
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Ozayn')),
      body: Column(
        children: [
          Expanded(
            child: ListView.builder(
              controller: _scrollCtrl,
              padding: const EdgeInsets.all(16),
              itemCount: _messages.length,
              itemBuilder: (ctx, i) {
                final m = _messages[i];
                final isUser = m['role'] == 'user';
                return Align(
                  alignment: isUser ? Alignment.centerRight : Alignment.centerLeft,
                  child: Container(
                    margin: const EdgeInsets.only(bottom: 8),
                    padding: const EdgeInsets.all(12),
                    constraints: BoxConstraints(maxWidth: MediaQuery.of(context).size.width * 0.75),
                    decoration: BoxDecoration(
                      color: isUser ? const Color(0xFF0A84FF).withOpacity(0.2) : const Color(0xFF1C1C38),
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: const Color(0x1AFFFFFF)),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          isUser ? 'You' : 'Ozayn',
                          style: TextStyle(
                            fontSize: 11,
                            fontWeight: FontWeight.bold,
                            color: isUser ? const Color(0xFF0A84FF) : const Color(0x80EBEBF5),
                          ),
                        ),
                        const SizedBox(height: 4),
                        Text(m['content'] ?? '', style: const TextStyle(fontSize: 14, color: Color(0xFFEBEBF5))),
                      ],
                    ),
                  ),
                );
              },
            ),
          ),
          if (_loading)
            const Padding(
              padding: EdgeInsets.all(8),
              child: SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2)),
            ),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: const BoxDecoration(
              color: Color(0xFF161632),
              border: Border(top: BorderSide(color: Color(0x1AFFFFFF))),
            ),
            child: Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _inputCtrl,
                    decoration: const InputDecoration(hintText: 'Type a message...'),
                    onSubmitted: (_) => _send(),
                  ),
                ),
                const SizedBox(width: 8),
                IconButton(
                  onPressed: _loading ? null : _send,
                  icon: const Icon(Icons.send, color: Color(0xFF0A84FF)),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _send() async {
    final msg = _inputCtrl.text.trim();
    if (msg.isEmpty || _loading) return;
    _inputCtrl.clear();

    setState(() {
      _messages.add({'role': 'user', 'content': msg});
      _loading = true;
    });

    _scrollToBottom();

    final result = await widget.api.sendMessage(msg, conversationId: _convId);

    if (mounted) {
      setState(() {
        _messages.add({'role': 'assistant', 'content': result['response'] ?? ''});
        _convId = result['conversation_id']?.toString() ?? _convId;
        _loading = false;
      });
      _scrollToBottom();
    }
  }

  void _scrollToBottom() {
    Future.delayed(const Duration(milliseconds: 100), () {
      if (_scrollCtrl.hasClients) {
        _scrollCtrl.animateTo(_scrollCtrl.position.maxScrollExtent, duration: const Duration(milliseconds: 300), curve: Curves.easeOut);
      }
    });
  }

  @override
  void dispose() {
    _inputCtrl.dispose();
    _scrollCtrl.dispose();
    super.dispose();
  }
}
