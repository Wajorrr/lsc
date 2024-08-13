# app.py
from flask import Flask, render_template
import json
import plotly
import plotly.express as px

app = Flask(__name__)

@app.route('/')
def index():
    # 使用 Plotly 创建图形
    df = px.data.iris()  # 使用内置的 iris 数据集
    fig = px.scatter(df, x='sepal_width', y='sepal_length', color='species')

    # 将图形转换为 JSON 格式
    graphJSON = json.dumps(fig, cls=plotly.utils.PlotlyJSONEncoder)

    return render_template('index.html', graphJSON=graphJSON)

if __name__ == '__main__':
    app.run(debug=True)