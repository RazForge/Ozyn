import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class OzaynApiClient {
  static const String baseUrl = 'http://10.0.2.2:8000/ozayn/backend/api';

  String? sessionId;
  Map<String, dynamic>? user;

  Map<String, String> get _headers => {
    'Content-Type': 'application/json',
    if (sessionId != null) 'X-Session-ID': sessionId!,
  };

  Future<Map<String, dynamic>> _request(String method, String endpoint, {Map<String, dynamic>? data}) async {
    final uri = Uri.parse('$baseUrl$endpoint');
    try {
      http.Response response;
      if (method == 'GET') {
        response = await http.get(uri, headers: _headers).timeout(const Duration(seconds: 15));
      } else if (method == 'POST') {
        response = await http.post(uri, headers: _headers, body: jsonEncode(data ?? {})).timeout(const Duration(seconds: 15));
      } else if (method == 'PUT') {
        response = await http.put(uri, headers: _headers, body: jsonEncode(data ?? {})).timeout(const Duration(seconds: 15));
      } else if (method == 'DELETE') {
        response = await http.delete(uri, headers: _headers).timeout(const Duration(seconds: 15));
      } else {
        return {'success': false, 'error': 'Invalid method'};
      }
      return jsonDecode(response.body);
    } catch (e) {
      return {'success': false, 'error': e.toString()};
    }
  }

  // Auth
  Future<Map<String, dynamic>> login(String username, String password) async {
    final result = await _request('POST', '/auth/login', data: {'username': username, 'password': password});
    if (result['success'] == true) {
      sessionId = result['session_id'];
      user = result['user'];
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('ozayn_session', sessionId!);
    }
    return result;
  }

  Future<Map<String, dynamic>> register(String username, String password, {String? email}) async {
    return await _request('POST', '/auth/register', data: {
      'username': username,
      'password': password,
      if (email != null) 'email': email,
    });
  }

  Future<bool> validateSession() async {
    try {
      final result = await _request('GET', '/chat/list');
      return result.containsKey('conversations');
    } catch (_) {
      return false;
    }
  }

  // Chat
  Future<Map<String, dynamic>> sendMessage(String message, {String? conversationId}) async {
    return await _request('POST', '/chat/send', data: {
      'message': message,
      if (conversationId != null) 'conversation_id': conversationId,
    });
  }

  Future<List<dynamic>> listConversations() async {
    final result = await _request('GET', '/chat/list');
    return result['conversations'] ?? [];
  }

  Future<List<dynamic>> getChatHistory(String conversationId) async {
    final result = await _request('GET', '/chat/history/$conversationId');
    return result['messages'] ?? [];
  }

  // Projects
  Future<List<dynamic>> listProjects() async {
    final result = await _request('GET', '/projects/list');
    return result['projects'] ?? [];
  }

  Future<Map<String, dynamic>> createProject(String name, {String? description}) async {
    return await _request('POST', '/projects/create', data: {
      'name': name,
      if (description != null) 'description': description,
    });
  }

  // Tasks
  Future<List<dynamic>> listTasks() async {
    final result = await _request('GET', '/tasks/list');
    return result['tasks'] ?? [];
  }

  Future<Map<String, dynamic>> createTask(String title, {String? priority}) async {
    return await _request('POST', '/tasks/create', data: {
      'title': title,
      if (priority != null) 'priority': priority,
    });
  }

  Future<Map<String, dynamic>> updateTask(dynamic taskId, String status) async {
    return await _request('PUT', '/tasks/$taskId', data: {'status': status});
  }

  // Knowledge
  Future<List<dynamic>> listKnowledge() async {
    final result = await _request('GET', '/knowledge/list');
    return result['results'] ?? [];
  }

  Future<Map<String, dynamic>> addKnowledge(String title, String content, {List<String>? tags}) async {
    return await _request('POST', '/knowledge/add', data: {
      'title': title,
      'content': content,
      if (tags != null) 'tags': tags,
    });
  }

  // ARWE
  Future<Map<String, dynamic>> arweStatus() async {
    return await _request('GET', '/arwe/status');
  }

  Future<Map<String, dynamic>> arweBriefing() async {
    return await _request('GET', '/arwe/briefing');
  }

  // Decisions
  Future<List<dynamic>> listDecisions() async {
    final result = await _request('GET', '/decisions/list');
    return result['decisions'] ?? [];
  }

  Future<Map<String, dynamic>> createDecision(String context) async {
    return await _request('POST', '/decisions/create', data: {'context': context});
  }

  // Audit
  Future<List<dynamic>> auditLog() async {
    final result = await _request('GET', '/audit/log');
    return result['log'] ?? [];
  }

  // System
  Future<Map<String, dynamic>> systemOverview() async {
    return await _request('GET', '/system/overview');
  }
}
