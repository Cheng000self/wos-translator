// API基础URL
const API_BASE = '';

// 会话管理
const SESSION_KEY = 'wos_session_token';

function getSessionToken() {
    return localStorage.getItem(SESSION_KEY);
}

function setSessionToken(token) {
    localStorage.setItem(SESSION_KEY, token);
}

function clearSessionToken() {
    localStorage.removeItem(SESSION_KEY);
}

function isAuthenticated() {
    return !!getSessionToken();
}

// API调用封装
async function apiCall(method, endpoint, data = null) {
    const options = {
        method: method,
        headers: {
            'Content-Type': 'application/json'
        }
    };
    
    // 添加会话token（如果存在）
    const token = getSessionToken();
    if (token) {
        options.headers['X-Session-Token'] = token;
    }
    
    if (data && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(data);
    }
    
    try {
        const response = await fetch(API_BASE + endpoint, options);
        const result = await response.json();
        
        // 如果返回401，清除会话并跳转到设置页面
        // 但对于 /api/settings/verify 端点，不要显示会话过期（这是密码验证失败）
        if (response.status === 401) {
            if (endpoint !== '/api/settings/verify') {
                clearSessionToken();
                if (window.location.pathname !== '/settings.html') {
                    showToast('会话已过期，请重新登录', 'warning');
                    setTimeout(() => {
                        window.location.href = '/settings.html';
                    }, 1500);
                }
                throw new Error('会话已过期');
            }
        }
        
        if (!response.ok) {
            throw new Error(result.error || '请求失败');
        }
        
        return result;
    } catch (error) {
        console.error('API调用失败:', error);
        throw error;
    }
}

// Toast通知
function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `fixed top-20 right-4 px-6 py-3 rounded-lg shadow-lg z-50 transition-opacity duration-300 ${
        type === 'success' ? 'bg-green-500 text-white' :
        type === 'error' ? 'bg-red-500 text-white' :
        type === 'warning' ? 'bg-yellow-500 text-white' :
        'bg-blue-500 text-white'
    }`;
    toast.textContent = message;
    
    document.body.appendChild(toast);
    
    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

// 确认对话框
function showConfirm(message, onConfirm) {
    const overlay = document.createElement('div');
    overlay.className = 'fixed inset-0 bg-black/50 flex items-center justify-center z-50';
    overlay.innerHTML = `
        <div class="bg-white rounded-lg p-6 max-w-md w-full mx-4">
            <h3 class="text-lg font-semibold text-slate-900 mb-4">确认操作</h3>
            <p class="text-slate-600 mb-6">${message}</p>
            <div class="flex justify-end space-x-3">
                <button class="cancel-btn px-4 py-2 text-slate-600 hover:bg-slate-100 rounded-lg transition-colors cursor-pointer">
                    取消
                </button>
                <button class="confirm-btn px-4 py-2 bg-blue-600 text-white hover:bg-blue-700 rounded-lg transition-colors cursor-pointer">
                    确认
                </button>
            </div>
        </div>
    `;
    
    document.body.appendChild(overlay);
    
    overlay.querySelector('.cancel-btn').addEventListener('click', () => {
        overlay.remove();
    });
    
    overlay.querySelector('.confirm-btn').addEventListener('click', () => {
        overlay.remove();
        onConfirm();
    });
    
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) {
            overlay.remove();
        }
    });
}

// 加载动画
function showLoading(message = '加载中...') {
    const loading = document.createElement('div');
    loading.id = 'loading-overlay';
    loading.className = 'fixed inset-0 bg-black/50 flex items-center justify-center z-50';
    loading.innerHTML = `
        <div class="bg-white rounded-lg p-6 flex items-center space-x-4">
            <div class="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
            <span class="text-slate-900">${message}</span>
        </div>
    `;
    
    document.body.appendChild(loading);
}

function hideLoading() {
    const loading = document.getElementById('loading-overlay');
    if (loading) {
        loading.remove();
    }
}

// 格式化日期
function formatDate(timestamp) {
    if (!timestamp) return '-';
    const date = new Date(timestamp * 1000);
    return date.toLocaleString('zh-CN');
}

// 格式化文件大小
function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

// 任务状态映射
const TaskStatus = {
    0: { text: '解析中', color: 'text-blue-600', bg: 'bg-blue-100' },
    1: { text: '等待中', color: 'text-gray-600', bg: 'bg-gray-100' },
    2: { text: '翻译中', color: 'text-yellow-600', bg: 'bg-yellow-100' },
    3: { text: '已暂停', color: 'text-orange-600', bg: 'bg-orange-100' },
    4: { text: '已完成', color: 'text-green-600', bg: 'bg-green-100' },
    5: { text: '失败', color: 'text-red-600', bg: 'bg-red-100' }
};

// 获取任务状态显示
function getTaskStatusDisplay(status) {
    return TaskStatus[status] || TaskStatus[5];
}

// 验证HTML文件
function validateHtmlFile(file) {
    // 检查文件扩展名
    const ext = file.name.toLowerCase().split('.').pop();
    if (ext !== 'html' && ext !== 'htm') {
        return { valid: false, error: '文件格式不正确，请上传HTML文件' };
    }
    
    // 检查文件大小（最大10MB）
    if (file.size > 10 * 1024 * 1024) {
        return { valid: false, error: '文件过大，最大支持10MB' };
    }
    
    return { valid: true };
}

// 读取文件内容
function readFileContent(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = (e) => resolve(e.target.result);
        reader.onerror = (e) => reject(e);
        reader.readAsText(file);
    });
}

// 验证WoS格式（支持英文和中文版）
function validateWosFormat(htmlContent) {
    const hasWosMarker = htmlContent.includes('Clarivate Web of Science') || 
                         htmlContent.includes('Web of Science');
    // 英文: "Record 1 of 500"  中文: "第 1 条，共 400 条"
    const hasRecordMarker = /Record \d+ of \d+/.test(htmlContent) ||
                            /第\s*\d+\s*条，共\s*\d+\s*条/.test(htmlContent);
    
    if (!hasWosMarker || !hasRecordMarker) {
        return { valid: false, error: '不是有效的Web of Science导出文件' };
    }
    
    return { valid: true };
}

// 估算文献数量（支持英文和中文版）
function estimateLiteratureCount(htmlContent) {
    // 英文: "500 record(s) printed from"
    const match = htmlContent.match(/(\d+) record\(s\) printed from/i);
    if (match) {
        return parseInt(match[1]);
    }
    
    // 英文: "Record 1 of 500"
    const recordMatch = htmlContent.match(/Record \d+ of (\d+)/);
    if (recordMatch) {
        return parseInt(recordMatch[1]);
    }
    
    // 中文: "第 1 条，共 400 条"
    const zhMatch = htmlContent.match(/第\s*\d+\s*条，共\s*(\d+)\s*条/);
    if (zhMatch) {
        return parseInt(zhMatch[1]);
    }
    
    return 0;
}

// 会话管理（用于设置页面）
const Session = {
    set: (key, value, expiresIn = 30 * 60 * 1000) => {
        const item = {
            value: value,
            expires: Date.now() + expiresIn
        };
        localStorage.setItem(key, JSON.stringify(item));
    },
    
    get: (key) => {
        const itemStr = localStorage.getItem(key);
        if (!itemStr) return null;
        
        const item = JSON.parse(itemStr);
        if (Date.now() > item.expires) {
            localStorage.removeItem(key);
            return null;
        }
        
        return item.value;
    },
    
    remove: (key) => {
        localStorage.removeItem(key);
    }
};
